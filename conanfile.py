import io as _io
import json as _json
import multiprocessing as _multiprocessing
import os as _os
import sys as _sys
from pathlib import Path as _Path
from urllib.parse import urlparse as _urlparse

import conan as _conan  # type: ignore
from conan.tools.cmake import CMakeDeps as _CMakeDeps  # type: ignore
from conan.tools.cmake import CMakeToolchain as _CMakeToolchain
from conan.tools.scm import Git as _Git  # type: ignore

# isort: off
# pylint: disable=import-error
# pylint: disable=wrong-import-position
# pylint: disable=no-member
_sys.path.append(str(_Path(__file__).parent / ".makepy"))
from support.manifest import ArchiveDependency as _ArchiveDependency  # type: ignore
from support.manifest import GitDependency as _GitDependency
from support.manifest import Manifest as _Manifest


class Package(_conan.ConanFile):  # type: ignore
    settings = "os", "arch", "build_type"
    options = None
    name = "openocd"
    default_options = None
    revision_mode = "scm"
    cmake_find_mode = "both"
    package_type = "application"

    exports = [
        ".makepy/conan/conanfile.json",
        ".makepy/support/manifest.json",
        ".makepy/support/manifest.py",
    ]

    exports_sources = [
        "*",
        # "!.git/*", # git is a part of current build system ¯\_(ツ)_/¯
        "!.makepy/artifacts/*",
        "!.mypy_cache/*",
        "!build/*",
        "!build-aux/*",
        "!external_sources/*",
    ]

    def requirements(self) -> None:
        conanfile_json = (
            _Path(__file__).parent / ".makepy" / "conan" / "conanfile.json"
        )
        with open(conanfile_json, "r", encoding="UTF-8") as file:
            deps = _json.loads(file.read())
        if self.settings.os != "Linux":  # type: ignore
            return
        # pylint: disable-next=not-callable
        self.test_requires(deps["riscv-gcc"])

    def set_version(self) -> None:
        source_folder = _Path(__file__).parent
        with _io.StringIO() as result:
            self.run(
                f"{source_folder}/make.py --no-history-dump --logging-level error conan-info version",
                stdout=result,
            )
            version = result.getvalue().splitlines()[-1].strip()

        self.version = version

    def layout(self) -> None:
        build_folder = _Path("build") / str(self.settings.build_type)  # type: ignore
        self.folders.generators = build_folder
        self.folders.build = build_folder

    def _manifest_path(self) -> _Path:
        return (
            _Path(self.source_folder) / ".makepy" / "support" / "manifest.json"
        )

    def _download_source_deps(self, destination: _Path) -> None:
        jobs = max(_multiprocessing.cpu_count(), 8)
        manifest = _Manifest(self._manifest_path())

        for dep in manifest:
            dep_dst = destination / dep.name
            if isinstance(dep, _GitDependency):
                shallow = not dep.full_clone
                shallow_args = None
                shallow_args_str = ""
                if shallow:
                    shallow_args = ["--depth", "1000"]
                    shallow_args_str = " ".join(shallow_args)
                _Git(self).clone(url=dep.url, target=dep_dst, args=shallow_args)
                if shallow:
                    _Git(self, folder=dep_dst).run(
                        f"fetch origin {dep.version}"
                    )
                _Git(self, folder=dep_dst).checkout(dep.version)
                _Git(self, folder=dep_dst).run(
                    f"submodule update --init --checkout --jobs {jobs} {shallow_args_str}"
                )

            elif isinstance(dep, _ArchiveDependency) and dep.url.startswith(
                "ftp://"
            ):
                parsed_url = _urlparse(dep.url)
                archive = _Path(parsed_url.path).name
                _conan.tools.files.ftp_download(
                    self, parsed_url.netloc, parsed_url.path
                )
                _conan.tools.files.unzip(
                    self,
                    archive,
                    destination=dep_dst,
                    strip_root=dep.strip_root,
                )
                _Path(archive).unlink()

            elif isinstance(dep, _ArchiveDependency):
                _conan.tools.files.get(
                    self,
                    dep.url,
                    destination=dep_dst,
                    strip_root=dep.strip_root,
                )

            else:
                assert False

            if dep.patch is not None:
                self.run(
                    f"patch --directory {dep_dst} --input {dep.patch} --strip 1"
                )

    def source(self) -> None:
        # NOTE: OpenOCD requires a dedicated "bootstrap" process. Usually this
        # involves calling of ./bootstrap script which is part of OpenOCD
        # source code. Currently, our conan/make.py build system initializes
        # submoudules separately and expect make.py-initiated bootstrap to be
        # launched as `./bootstrap nosubmodule`
        self.run("git submodule init")
        self.run("git submodule update")

    def generate(self) -> None:
        toolchain = _CMakeToolchain(self)

        external_deps_folder = (
            _Path(self.generators_folder) / "external_dependencies"
        )
        self._download_source_deps(external_deps_folder / "sources")
        # TODO: remove this once conan is integrated into spike and riscb-binutils-gdb
        spike_urls = {
            "Ubuntu": (
                "http://artifactory.dev.syntacore.com:8082/artifactory/tools-gitlab-artifacts/"
                "spike/sc_main/230426-120908_b58d783f/spike-23_04_26-x86_64-ubuntu-18.04-b58d783fb0e2.tar.gz"
            ),
            "CentOS": (
                "http://artifactory.dev.syntacore.com:8082/artifactory/tools-gitlab-artifacts/"
                "spike/sc_main/230426-120908_b58d783f/spike-23_04_26-x86_64-centos-7-b58d783fb0e2.tar.gz"
            ),
        }
        riscv_binutils_gdb_url = (
            "http://artifactory.dev.syntacore.com:8082/artifactory/tools-gitlab-artifacts/"
            "riscv-binutils-gdb/197d5a51/x86_Lin-x86_Lin-RISCV64_Elf_binutils-gdb.tar.gz"
        )
        if self.settings.os == "Linux":  # type: ignore
            spike_url = spike_urls[str(self.settings.os.distro)]  # type: ignore
            _conan.tools.files.get(
                self,
                spike_url,
                destination=external_deps_folder,
                strip_root=True,
            )
            _conan.tools.files.get(
                self, riscv_binutils_gdb_url, destination=external_deps_folder
            )
            # FIXME: can we enforce **normalized** absolute paths here?
            toolchain.variables[
                "RISCVSpike_DIR"
            ] = f"{external_deps_folder}/spike"
            toolchain.variables[
                "RISCVGDB_DIR"
            ] = f"{external_deps_folder}/binutils-gdb"
            toolchain.variables["CMAKE_BUILD_TYPE"] = self.settings.build_type  # type: ignore

        toolchain.generate()
        deps = _CMakeDeps(self)
        deps.generate()

    def build(self) -> None:
        self.run(
            f"{self.source_folder}/make.py --no-history-dump config --build-path {self.build_folder}"
        )
        self.run(
            f"{self.source_folder}/make.py --no-history-dump build --build-path {self.build_folder} --target openocd"
        )

    def package(self) -> None:
        _conan.tools.files.copy(
            self,
            "*",
            f"{self.build_folder}/install_openocd/openocd",
            self.package_folder,
        )

    def package_info(self) -> None:
        # TODO: regarding this **RISCV_OPENOCD_DIR** name.
        # It's better to rename this to something like SC_RISCV_OPENOCD_DIR.
        # Any other naming scheme (like omitting RISCV from the prefix) may
        # introduce an unnecessary confusion in client codebase.
        self.buildenv_info.define("RISCV_OPENOCD_DIR", self.package_folder)
        self.runenv_info.define("RISCV_OPENOCD_DIR", self.package_folder)
        self.cpp_info.includedirs = []  # no includes
        self.cpp_info.libdirs = []  # no libraries to link against
