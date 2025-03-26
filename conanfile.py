from pathlib import Path

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


# pylint: disable=no-member,not-callable
class Package(ConanFile):  # type: ignore
    name = "openocd"
    settings = "os", "arch", "build_type", "compiler"
    options = {
        "source": [None, "internal", "syntacore"],  # TODO: "riscv", "mainline"
        "sanitize": ["disable", "enable", "strict"],
        "elct_support": [None, True, False],  # Legacy option
    }
    default_options = {
        "source": None,
        "sanitize": "disable",
        "elct_support": None,
    }
    package_type = "application"
    url = "<default_remote_git_service>/tools/toolchain/openocd"

    python_requires = "makepy_hints/1.19.0-rc.0.12+sc.main@sc/main"
    python_requires_extend = "makepy_hints.MakepyConanFile"

    mp_git_clone_depth = 2000  # We need some history to find the merge base

    def configure(self) -> None:
        match self.mp_opts.as_str("elct_support"):
            case "True":
                self.options.source = "internal"  # type: ignore
            case "False":
                self.options.source = "syntacore"  # type: ignore
        self.options.rm_safe("elct_support")  # type: ignore

    def package_id(self) -> None:
        self.info.settings.rm_safe("compiler")

    def requirements(self) -> None:
        self.tool_requires("libtool")
        self.tool_requires("pkgconf")

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
