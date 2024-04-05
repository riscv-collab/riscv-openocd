# type: ignore
import io as _io
import json as _json
import multiprocessing as _multiprocessing
import os as _os
import sys as _sys
from pathlib import Path as _Path
from urllib.parse import urlparse as _urlparse

import conan as _conan
from conan.tools.cmake import CMakeToolchain as _CMakeToolchain
from conan.tools.scm import Git as _Git

# isort: off
# pylint: disable=import-error
# pylint: disable=wrong-import-position
# pylint: disable=no-member
_sys.path.append(str(_Path(__file__).parent / ".makepy"))


class Package(_conan.ConanFile):
    name = "openocd"
    settings = "os", "arch"
    options = {"test": [True, False], "build_type": ["Release", "Debug"]}
    default_options = {"test": False, "build_type": "Release"}
    revision_mode = "scm"
    cmake_find_mode = "both"
    package_type = "application"
    url = "https://gitlab.dev.syntacore.com/tools/toolchain/openocd"

    exports = [
        "conandeps.json",
    ]

    exports_sources = [
        "*",
        "!.git/*",
        "!.makepy/artifacts/*",
        "!.mypy_cache/*",
        "!build/*",
        "!build-aux/*",
        "!external_sources/*",
    ]

    def set_name(self) -> None:
        self.name = self.name or "openocd"  # type: ignore

        source_folder = _Path(__file__).parent

        commit_msg_io_string = _io.StringIO()
        self.run(
            "git log -1 --oneline --format=%B",
            cwd=source_folder,
            stdout=commit_msg_io_string,
        )
        commit_message = commit_msg_io_string.getvalue().strip()

        merge_base_io_string = _io.StringIO()
        self.run(
            "git merge-base origin/riscv HEAD",
            cwd=source_folder,
            stdout=merge_base_io_string,
        )
        riscv_merge_base = merge_base_io_string.getvalue().strip()[:8]

        is_clean_io_string = _io.StringIO()
        self.run(
            "git status --porcelain",
            cwd=source_folder,
            stdout=is_clean_io_string,
        )
        dirty_marker = "" if is_clean_io_string.getvalue() == "" else "-dirty"

        relstr_prefix = "SYNTACORE_RELSTR: "
        commit_message_lines = [s.strip() for s in commit_message.splitlines()]
        release_string = next(
            filter(lambda s: s.startswith(relstr_prefix), commit_message_lines),
            "",
        )
        release_string = release_string.removeprefix(relstr_prefix).strip()

        commit_hash = _Git(self).get_commit()[:8]
        release_string = f"{release_string}-g{commit_hash}{dirty_marker}"
        version_info = (
            f"riscv-upstream-{riscv_merge_base}-cs-{commit_hash}{dirty_marker}"
        )

        with open(
            source_folder / "__sc_version.txt", "w", encoding="utf-8"
        ) as version_file:
            version_file.write(f"{release_string}\n{version_info}")

    # pylint: disable=not-callable
    def requirements(self) -> None:
        conanfile_json = _Path(__file__).parent / "conandeps.json"
        with open(conanfile_json, "r", encoding="UTF-8") as file:
            deps = _json.loads(file.read())

        self.requires(deps["openocd_source_deps"])

        if self.settings.os != "Linux":
            return
        if self.options.test != "True":
            return

        self.test_requires(deps["external_openocd_tests"])
        self.test_requires(deps["riscv-gcc"])
        self.test_requires(deps["riscv-gdb"])
        self.test_requires(deps["riscv-isa-sim"])
        self.test_requires(deps["dejagnu"])

    def layout(self) -> None:
        build_folder = _Path("build") / str(self.options.build_type)
        self.folders.generators = build_folder
        self.folders.build = build_folder

    def source(self) -> None:
        # NOTE: OpenOCD requires a dedicated "bootstrap" process. Usually this
        # involves calling of ./bootstrap script which is part of OpenOCD
        # source code. Currently, our conan/make.py build system initializes
        # submoudules separately and expect make.py-initiated bootstrap to be
        # launched as `./bootstrap nosubmodule`
        if _os.path.isdir(_Path(self.source_folder) / ".git"):
            self.run("git submodule init")
            self.run("git submodule update")

    def _var(self, name: str) -> str:
        for _, info in self.dependencies.items():
            run_vars = info.runenv_info.vars(self)
            for var, val in run_vars.items():
                if var == name:
                    return str(val)
        assert False

    def generate(self) -> None:
        toolchain = _CMakeToolchain(self)

        toolchain.variables["OPENOCD_SOURCE_DEPS_DIR"] = self._var(
            "SC_OPENOCD_SOURCE_DEPS_PATH"
        )

        if self.settings.os == "Linux" and self.options.test:
            toolchain.variables["RISCVSpike_DIR"] = self._var("SC_SPIKE_PATH")
            toolchain.variables["RISCVGCC_DIR"] = self._var("SC_GCC_PATH")
            toolchain.variables["RISCVGDB_DIR"] = self._var("SC_RISCV_GDB_PATH")
            toolchain.variables["DEJAGNU_DIR"] = self._var("SC_DEJAGNU_PATH")
            toolchain.variables["RISCVTESTS_DIR"] = self._var(
                "SC_EXTERNAL_OPENOCD_TESTS_PATH"
            )
            toolchain.variables["CMAKE_BUILD_TYPE"] = self.options.build_type
            toolchain.variables["SC_OPENOCD_ENABLE_TESTS"] = "ON"

        toolchain.generate()

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

        self.buildenv_info.define("SC_RISCV_OPENOCD_PATH", self.package_folder)
        self.runenv_info.define("SC_RISCV_OPENOCD_PATH", self.package_folder)

        self.cpp_info.includedirs = []  # no includes
        self.cpp_info.libdirs = []  # no libraries to link against
