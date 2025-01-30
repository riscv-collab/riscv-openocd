# type: ignore
# pylint: disable=no-member,pointless-statement,invalid-name,not-callable,cyclic-import
from pathlib import Path

from conan import ConanFile
from conan.tools.gnu import Autotools, AutotoolsToolchain, PkgConfigDeps
from conan.tools.scm import Git

_shared_configure_args = [
    "--enable-amtjtagaccel",
    "--enable-armjtagew",
    "--enable-aice",
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
    "--disable-internal-libjaylink",
    "--disable-internal-jimtcl",  # won't be neccessary soon
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


class Package(ConanFile):
    name = "openocd"
    settings = "os", "arch", "build_type", "compiler"
    options = {
        "source": ["internal", "syntacore"],  # TODO: "riscv", "mainline"
        "sanitize": ["disable", "enable", "strict"],
        "elct_support": [None, True, False],
    }
    default_options = {
        "source": "internal",
        "sanitize": "disable",
        "elct_support": None,
    }
    package_type = "application"
    url = "<default_remote_git_service>/tools/toolchain/openocd"

    python_requires = "makepy_hints/1.15.0-rc.0.10+sc.main@sc/main"
    python_requires_extend = "makepy_hints.MakepyConanFile"

    mp_git_clone_depth = 2000  # We need some history to find the merge base

    def configure(self):
        if self.options.get_safe("elct_support"):  # Legacy option
            self.options.rm("elct_support")
            self.options["source"] = "internal"

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

        if self.options.source == "internal" and self.settings.os == "Linux":
            self.requires("jansson", options={"shared": False})

    def layout(self) -> None:
        build_folder = Path("build") / str(self.settings.build_type)
        self.folders.generators = build_folder
        self.folders.build = build_folder

    def generate(self) -> None:
        riscv_url = "https://github.com/riscv-collab/riscv-openocd.git"
        git = Git(self)
        git.run(f"fetch {riscv_url}")
        self.mp_hints.custom["riscv_rev"] = git.run(
            "rev-parse FETCH_HEAD"
        ).strip()
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
        match self.settings.os:
            case "Linux":
                extra_configure_args = _linux_configure_args
            case "Windows":
                extra_configure_args = _windows_configure_args
            case _:
                self.output.error(f"Unexpected host OS '{self.settings.os}'")

        for configure_arg in extra_configure_args:
            ac.configure_args.append(configure_arg)

        if (
            self.options.source == "internal" and self.settings.os == "Linux"
        ):  # TODO: just build form other repo
            ac.configure_args.append("--enable-syntacore-extensions")

        if self.options.sanitize != "disable":
            ac.extra_cflags.extend(["-fsanitize=undefined", "-Wl,-ldl"])
            if self.options.sanitize == "strict":
                ac.extra_cflags.append("-fno-sanitize-recover")

        match self.settings.build_type:
            case "Release":
                ac.extra_cflags.append("-O2")
                # FIXME: YCAT-43010
                ac.ndebug = False
            case "Debug":
                ac.extra_cflags.extend(["-O0", "-g"])
            case _:
                self.output.error(
                    f"Unexpected build_type '{self.settings.build_type}'"
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
