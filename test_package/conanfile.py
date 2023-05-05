from conan import ConanFile as _ConanFile  # type: ignore
from conan.tools.build import can_run as _can_run  # type: ignore


class TestPackage(_ConanFile):  # type: ignore
    settings = "os", "compiler", "build_type"
    generators = "VirtualRunEnv"

    def requirements(self) -> None:
        self.requires(self.tested_reference_str)

    def layout(self) -> None:
        self.folders.generators = "build"
        self.folders.build = "build"

    def build(self) -> None:
        pass

    def test(self) -> None:
        if _can_run(self):
            self.run("$RISCV_OPENOCD_DIR/bin/openocd --version", env="conanrun")
