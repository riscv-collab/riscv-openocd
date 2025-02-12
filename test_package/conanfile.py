import io

from conan import ConanFile  # type: ignore
from conan.tools.build import can_run  # type: ignore


class TestPackage(ConanFile):  # type: ignore
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
        if can_run(self):
            with io.StringIO() as out_stream:
                self.run(
                    "$RISCV_OPENOCD_DIR/bin/openocd --version 2>&1",
                    env="conanrun",
                    stdout=out_stream,
                )
                version_string = out_stream.getvalue()
            print(f"version: {version_string}")
            if "dirty" in version_string:
                raise ValueError("unexpected dirty version")
