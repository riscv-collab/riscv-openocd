# type: ignore
# pylint: disable=no-member,pointless-statement,invalid-name,not-callable,cyclic-import
import re
from pathlib import Path

import conan
from conan.tools.cmake import CMakeToolchain
from conan.tools.gnu import PkgConfigDeps
from conan.tools.scm import Git


class Package(conan.ConanFile):
    name = "openocd"
    settings = "os", "arch", "build_type"
    options = {"test": [True, False], "elct_support": [True, False]}
    default_options = {"test": False, "elct_support": False}
    package_type = "application"
    url = "<default_remote_git_service>/tools/toolchain/openocd"

    python_requires = "makepy_hints/1.15.0-rc.0.10+sc.main@sc/main"
    python_requires_extend = "makepy_hints.MakepyConanFile"

    def set_name(self) -> None:
        source_folder = Path(__file__).parent

        git = Git(self)

        dirty_marker = "-dirty" if git.is_dirty() else ""

        split_version = str(self.version).split("+", maxsplit=1)
        version_string = split_version[1] if len(split_version) > 1 else ""
        version_string = re.sub(r"[\W_]+", "_", version_string).strip()
        if version_string == "":
            version_string = "development_build"

        commit_hash = git.get_commit()[:8]
        release_string = f"{version_string}-g{commit_hash}{dirty_marker}"

        riscv_url = "https://github.com/riscv-collab/riscv-openocd.git"
        git.run(f"fetch {riscv_url}")
        riscv_merge_base = git.run("merge-base FETCH_HEAD HEAD").strip()[:8]

        version_info = (
            f"riscv-upstream-{riscv_merge_base}-cs-{commit_hash}{dirty_marker}"
        )

        with open(
            source_folder / "__sc_version.txt", "w", encoding="utf-8"
        ) as version_file:
            version_file.write(f"{release_string}\n{version_info}")

    def requirements(self) -> None:
        self.requires("libusb", options={"shared": False})
        #'libftdi' depends on 'libusb'
        self.requires(
            "libftdi",
            options={
                "shared": False,
                "enable_cpp_wrapper": False,
                "use_streaming": False,
            },
        )
        #'hidapi' depends on 'libusb'
        self.requires("hidapi", options={"shared": False})
        self.requires(
            "libjaylink",
            options={
                "shared": False,
                "subproject-build": "disable",
                "libusb": "with",
                "extra_cflags": "-O2",
            },
        )
        self.requires(
            "jimtcl",
            options={
                "with-ext": "json",
                "minimal": "",
                "ssl": False,
                "extra_cflags": "-O2",
            },
        )

        if self.settings.os != "Linux":
            return

        if self.options.elct_support:
            self.requires("jansson", options={"shared": False})

        if self.options.test != "True":
            return

        self.test_requires("external_openocd_tests")
        self.test_requires("riscv-gcc")
        self.test_requires("riscv-gdb")
        self.test_requires("riscv-isa-sim")
        self.test_requires("dejagnu")

    def layout(self) -> None:
        build_folder = Path("build") / str(self.settings.build_type)
        self.folders.generators = build_folder
        self.folders.build = build_folder

    def generate(self) -> None:
        toolchain = CMakeToolchain(self)

        def set_toolchain_var_from_host_hint(tc_var, dep, hint_var) -> None:
            toolchain.variables[tc_var] = self.mp_hints.host[dep].vars[hint_var]

        toolchain.variables["CMAKE_BUILD_TYPE"] = self.settings.build_type

        if self.settings.os == "Linux" and self.options.test:
            for tc_var, dep, hint_var in [
                ("RISCVSpike_DIR", "riscv-isa-sim", "SC_SPIKE_PATH"),
                ("RISCVGCC_DIR", "riscv-gcc", "SC_GCC_PATH"),
                ("RISCVGDB_DIR", "riscv-gdb", "SC_RISCV_GDB_PATH"),
                ("DEJAGNU_DIR", "dejagnu", "SC_DEJAGNU_PATH"),
                (
                    "RISCVTESTS_DIR",
                    "external_openocd_tests",
                    "SC_EXTERNAL_OPENOCD_TESTS_PATH",
                ),
            ]:
                set_toolchain_var_from_host_hint(tc_var, dep, hint_var)
            toolchain.variables["SC_OPENOCD_ENABLE_TESTS"] = "ON"

        if self.options.elct_support:
            toolchain.variables["ENABLE_ELCT_SUPPORT"] = "ON"

        toolchain.generate()

        pc = PkgConfigDeps(self)
        pc.generate()
        # hidapi is included using "hidapi.h", not "hidapi/hidapi.h".
        hidapi_pc_path = Path(self.build_folder) / "hidapi.pc"
        hidapi_pc_data = hidapi_pc_path.read_text(encoding="utf-8")
        hidapi_pc_data = hidapi_pc_data.replace(
            "${includedir}", "${includedir}/hidapi"
        )
        hidapi_pc_path.write_text(hidapi_pc_data, encoding="utf-8")

    def build(self) -> None:
        self.run(
            f"{self.source_folder}/make.py --no-history-dump config --build-path {self.build_folder}"
        )
        self.run(
            f"{self.source_folder}/make.py --no-history-dump build --build-path {self.build_folder} --target openocd"
        )

    def package(self) -> None:
        conan.tools.files.copy(
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
