# type: ignore
from pathlib import Path

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain
from conan.tools.files import copy


class Package(ConanFile):
    url = "<default_remote_git_service>/tools/toolchain/openocd"

    python_requires = "makepy_hints/1.15.0-rc.0.10+sc.main@sc/main"
    python_requires_extend = "makepy_hints.MakepyConanFile"

    def requirements(self) -> None:
        self.requires("external_openocd_tests")
        self.requires("riscv-gcc")
        self.requires("riscv-gdb")
        self.requires("riscv-isa-sim")
        self.requires("dejagnu")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def package(self):
        copy(
            self,
            "*",
            Path(self.source_folder) / "testsuite",
            Path(self.package_folder) / "testsuite",
            excludes="conanfile.py",
        )
        copy(
            self,
            "*",
            Path(self.source_folder) / "testing" / "dejagnu",
            Path(self.package_folder) / "syntacore",
        )
