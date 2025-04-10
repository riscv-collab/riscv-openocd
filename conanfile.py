from abc import ABC, abstractmethod
from pathlib import Path
from typing import Any, Iterable, Mapping

from conan import ConanFile  # type: ignore
from conan.errors import ConanException  # type: ignore
from conan.tools.gnu import (  # type: ignore
    Autotools,
    AutotoolsToolchain,
    PkgConfigDeps,
)
from conan.tools.scm import Git  # type: ignore

_shared_configure_args = [
    "--enable-amtjtagaccel",
    "--enable-armjtagew",
    "--enable-angie",
    "--enable-cmsis-dap",
    "--enable-ftdi",
    "--enable-jlink",
    "--enable-jtag_vpi",
    "--enable-gw16012",
    "--enable-opendous",
    "--enable-parport",
    "--enable-usb-blaster-2",
    "--enable-ulink",
    "--enable-osbdm",
    "--enable-stlink",
    "--enable-ti-icdi",
    "--enable-rlink",
    "--enable-remote-bitbang",
    "--enable-usbprog",
    "--enable-vsllink",
    "--disable-parport-ppdev",
]


_linux_configure_args = _shared_configure_args + [
    "--enable-verbose",
    "--enable-verbose-usb-io",
    "--enable-verbose-usb-comms",
    "--enable-rshim",
    "--enable-ftdi-cjtag",
    "--enable-ft232r",
    "--enable-xds110",
    "--enable-cmsis-dap-v2",
    "--enable-esp-usb-jtag",
    "--enable-nulink",
    "--enable-kitprog",
    "--enable-usb-blaster",
    "--enable-presto",
    "--enable-openjtag",
    "--enable-buspirate",
    "--enable-vdebug",
    "--enable-jtag_dpi",
    "--enable-bcm2835gpio",
    "--enable-imx_gpio",
    "--enable-am335xgpio",
    "--enable-ep93xx",
    "--enable-at91rm9200",
    "--enable-sysfsgpio",
    "--enable-xlnx-pcie-xvc",
]


_windows_configure_args = _shared_configure_args + [
    "--disable-werror",
    "--enable-riscv",
    "--enable-openjtag_ftdi",
    "--enable-legacy-ft2232_libftdi",
    "--enable-parport-giveio",
    "--enable-presto_libftdi",
    "--enable-usb_blaster_libftdi",
    "--enable-target64",
]


class _Option(ABC):
    @property
    @abstractmethod
    def application_order(self) -> int:
        pass

    name: str

    def to_conan_default(self) -> tuple[str, None]:
        return (self.name, None)

    @abstractmethod
    def to_conan_option(self) -> tuple[str, Iterable[str | None]]:
        pass

    @abstractmethod
    def apply(self, recipe: Any) -> None:
        pass


class _OptionAlias(_Option):
    application_order = 0

    def __init__(
        self, name: str, *, final_name: str, mapping: Mapping[str, str]
    ):
        self.name = name
        self.final_name = final_name
        self.mapping = mapping

    def to_conan_option(self) -> tuple[str, Iterable[str | None]]:
        return (self.name, [None, *self.mapping.keys()])

    def apply(self, recipe: Any) -> None:
        if recipe.mp_opts.as_str_or_none(self.name) is not None:
            setattr(
                recipe.options,
                self.final_name,
                self.mapping[recipe.mp_opts.as_str(self.name)],
            )
        recipe.options.rm_safe(self.name)


class _OptionPreset(_Option):
    application_order = 1
    name = "preset"

    def __init__(self, presets: Mapping[str, Mapping[str, str]]):
        self._presets = presets

    def to_conan_option(self) -> tuple[str, Iterable[str | None]]:
        return (self.name, [None, *self._presets.keys()])

    def apply(self, recipe: Any) -> None:
        preset_name = recipe.mp_opts.as_str_or_none(self.name)
        recipe.options.rm_safe(self.name)
        if preset_name is None:
            return

        if any(
            recipe.mp_opts.as_str_or_none(option)
            != recipe.default_options[option]
            for option in recipe.mp_opts.keys()
        ):
            raise ConanException(
                "Either specify a preset or any of the other options."
            )

        for option, value in self._presets[preset_name].items():
            setattr(recipe.options, option, value)


class _FinalOption(_Option):
    application_order = 2

    def __init__(self, name: str, *, values: Iterable[str], default: str):
        assert default in values
        self.name = name
        self.values = values
        self.default = default

    def to_conan_option(self) -> tuple[str, Iterable[str | None]]:
        return (self.name, set([self.default, *self.values]))

    def apply(self, recipe: Any) -> None:
        if recipe.mp_opts.as_str_or_none(self.name) is None:
            setattr(recipe.options, self.name, self.default)


_options = [
    _OptionAlias(
        "elct_support",
        final_name="source",
        mapping={
            "True": "internal",
            "true": "internal",
            "False": "syntacore",
            "false": "syntacore",
            "": "syntacore",
        },
    ),
    _OptionAlias(
        "release",
        final_name="preset",
        mapping={
            "internal": "internal_release",
            "external": "external_release",
        },
    ),
    _OptionPreset(
        {
            "internal_release": {
                "source": "internal",
            },
            "external_release": {
                "source": "syntacore",
            },
        }
    ),
    _FinalOption(
        "source",
        values=["internal", "syntacore"],
        default="internal",
    ),
    _FinalOption(
        "sanitize",
        values=["disable", "enable", "strict"],
        default="disable",
    ),
]
assert sorted(_options, key=lambda o: o.application_order) == _options


# pylint: disable=no-member,not-callable
class Package(ConanFile):  # type: ignore
    name = "openocd"
    settings = "os", "arch", "build_type", "compiler"
    options_description = {
        "preset": "Select a preset for all other options. Once a preset is selected, specifying any other option is an error."
    }
    options = dict(o.to_conan_option() for o in _options)
    default_options = dict(o.to_conan_default() for o in _options)
    package_type = "application"
    url = "<default_remote_git_service>/tools/toolchain/openocd"

    python_requires = "makepy_hints/1.19.0-rc.0.12+sc.main@sc/main"
    python_requires_extend = "makepy_hints.MakepyConanFile"

    mp_git_clone_depth = 2000  # We need some history to find the merge base

    def configure(self) -> None:
        for option in _options:
            option.apply(self)

    def package_id(self) -> None:
        self.info.settings.rm_safe("compiler")

    def requirements(self) -> None:
        self.tool_requires("libtool")
        self.tool_requires("pkgconf")
        self.tool_requires("openocd_platform_configs")

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

        if self.options.source == "internal" and self.settings.os == "Linux":  # type: ignore
            self.requires("jansson", options={"shared": False})

    def layout(self) -> None:
        build_folder = Path("build") / str(self.settings.build_type)  # type: ignore

        self.folders.generators = build_folder
        self.folders.build = build_folder

    def generate(self) -> None:
        git = Git(self)
        git.run(f"fetch origin riscv --depth={self.mp_git_clone_depth}")
        pc = PkgConfigDeps(self)
        pc.generate()
        # hidapi is included using "hidapi.h", not "hidapi/hidapi.h".
        hidapi_pc_path = Path(self.build_folder) / "hidapi.pc"
        hidapi_pc_data = hidapi_pc_path.read_text(encoding="utf-8")
        hidapi_pc_data = hidapi_pc_data.replace(
            "${includedir}", "${includedir}/hidapi"
        )
        hidapi_pc_path.write_text(hidapi_pc_data, encoding="utf-8")

        ac = AutotoolsToolchain(self)

        match self.settings.os:  # type: ignore
            case "Linux":
                extra_configure_args = _linux_configure_args
            case "Windows":
                extra_configure_args = _windows_configure_args
            case _:
                raise ConanException(f"Unexpected host OS '{self.settings.os}'")  # type: ignore

        for configure_arg in extra_configure_args:
            ac.configure_args.append(configure_arg)

        if (
            self.mp_opts.as_str("source") == "internal" and self.settings.os == "Linux"  # type: ignore
        ):  # TODO: just build form other repo
            ac.configure_args.append("--enable-syntacore-extensions")

        if self.mp_opts.as_str("sanitize") != "disable":
            ac.extra_cflags.extend(["-fsanitize=undefined", "-Wl,-ldl"])
            if self.mp_opts.as_str("sanitize") == "strict":
                ac.extra_cflags.append("-fno-sanitize-recover")

        match self.settings.build_type:  # type: ignore
            case "Release":
                ac.extra_cflags.append("-O2")
                # FIXME: YCAT-43010
                ac.ndebug = False
            case "Debug":
                ac.extra_cflags.extend(["-O0", "-g"])
            case _:
                raise ConanException(
                    f"Unexpected build_type '{self.settings.build_type}'"  # type: ignore
                )

        ac.generate()

    def build(self) -> None:
        self.run("./bootstrap nosubmodule", cwd=self.source_folder)
        autotools = Autotools(self)
        autotools.autoreconf()
        autotools.configure()
        autotools.make()

    def package(self) -> None:
        autotools = Autotools(self)
        autotools.install()

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
