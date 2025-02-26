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
        # FIXME: figure out how to differentiate two different packages here
        pass
