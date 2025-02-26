# type: ignore
from pathlib import Path

from conan import ConanFile
from conan.tools.files import copy


class Package(ConanFile):
    url = "<default_remote_git_service>/tools/toolchain/openocd"

    python_requires = "makepy_hints/1.15.0-rc.0.10+sc.main@sc/main"
    python_requires_extend = "makepy_hints.MakepyConanFile"

    def layout(self) -> None:
        self.folders.root = ".."

    def requirements(self) -> None:
        self.requires("external_openocd_tests")
        self.requires("riscv-gcc")
        self.requires("riscv-gdb")
        self.requires("riscv-isa-sim")
        self.requires("dejagnu")

    def package(self):
        copy(
            self,
            "*",
            Path(self.source_folder) / "testsuite",
            self.package_folder,
        )
