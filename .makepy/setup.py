#!/usr/bin/env python3

import logging
from argparse import ArgumentParser, Namespace
from pathlib import Path

from _ocd_test_suite import _OcdTestSuite
from hwrs import HWRSSuite
from makepy import Command, Conductor
from makepy.conan import BinaryPackage, Conan, MakepyHintsHook, RevisionedRef
from makepy.generic import GenericSuite, ParallelHook, PrivilegedContainerHook
from makepy.lint import FormatCommand, LintCommand
from makepy.syntacore import ConanConfigs, ConanSuite, SyntacoreSuite
from makepy.utils import git
from makepy.utils import main as _main_decorator
from makepy.utils import run_shell

_logger = logging.getLogger()
_repo_path = Path(__file__).parent.parent


class _ConfigCommand(Command):
    def name(self) -> str:
        return "config"

    def help(self) -> str:
        return "CMake config."

    def amend_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument(
            "--tests-adapter-info",
            dest="tests_adapter_info",
            type=str,
            default=None,
            help="json file with debug adapter properties",
        )
        parser.add_argument(
            "--tests-valgrind-path",
            dest="tests_valgrind_path",
            type=str,
            default=None,
            help="run OpenOCD under valgrid when running tests",
        )
        parser.add_argument(
            "--openocd-install",
            dest="openocd_install",
            type=str,
            required=True,
            help="path to OpenOCD install",
        )

    def command(self, args: Namespace) -> None:
        cmd = [
            "cmake",
            "-S",
            _repo_path / ".makepy" / "support",
            "-B",
            args.build_path,
            "--toolchain",
            (args.build_path / "conan_toolchain.cmake").absolute(),
        ]

        if args.tests_adapter_info is not None:
            cmd.extend(
                [
                    f"-DOPENOCD_TESTS_DEBUG_ADAPTER_INFO={args.tests_adapter_info}"
                ]
            )

        if args.tests_valgrind_path:
            valgrind_abs_path = args.tests_valgrind_path
            if not Path(args.tests_valgrind_path).is_absolute():
                valgrind_abs_path = run_shell(
                    ["which", args.tests_valgrind_path],
                    capture_output=True,
                    loglevel=logging.DEBUG,
                ).stdout.strip()
            cmd.extend([f"-DOPENOCD_TESTS_VALGRIND_PATH={valgrind_abs_path}"])

        cmd.extend(
            [
                f"-DRISCVSpike_DIR={args.hints.host['riscv-isa-sim'].vars['SC_SPIKE_PATH']}",
                f"-DRISCVGCC_DIR={args.hints.host['riscv-gcc'].vars['SC_GCC_PATH']}",
                f"-DRISCVGDB_DIR={args.hints.host['riscv-gdb'].vars['SC_RISCV_GDB_PATH']}",
                f"-DDEJAGNU_DIR={args.hints.host['dejagnu'].vars['SC_DEJAGNU_PATH']}",
                f"-DRISCVTESTS_DIR={args.hints.host['external_openocd_tests'].vars['SC_EXTERNAL_OPENOCD_TESTS_PATH']}",
                f"-DOPENOCD_INSTALL_PATH={Path(args.openocd_install).resolve()}",
            ]
        )
        run_shell(cmd)


class _BuildCommand(Command):
    def name(self) -> str:
        return "build"

    def help(self) -> str:
        return "CMake build."

    def amend_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument(
            "-t",
            "--target",
            dest="cmake_target",
            type=str,
            required=True,
            help="Build target for CMake.",
        )

    def command(self, args: Namespace) -> None:
        cmd = [
            "cmake",
            "--build",
            args.build_path,
            "--target",
            args.cmake_target,
            "--parallel",
            str(args.parallel),
        ]
        if args.logging_level == "DEBUG":
            cmd.append("--verbose")
        run_shell(cmd)


class _PrintPackageURLs(Command):
    """
    WARNING: NEVER CALL THIS FROM THE CONAN RECIPE
    """

    def name(self) -> str:
        return "print-package-urls"

    def help(self) -> str:
        return "Search for package URLs in the Conan registry."

    def amend_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument(
            "--assume-release",
            action="store_true",
            help="Check for release packages instead of experimental ones.",
        )

    def _format_info(self, binary: BinaryPackage) -> str:
        os_str = f"{binary.settings.build_type} {binary.settings.arch}"
        if binary.settings.os and binary.settings.os.name == "Linux":
            os_or_distro = binary.settings.os.distro or binary.settings.os.name
            os_str = f"{os_str} {os_or_distro} {binary.settings.os.version}"
        elif binary.settings.os:
            os_str = f"{os_str} {binary.settings.os.name}"
        for key, value in binary.options.items():
            key = key.strip()
            value = value.strip()
            if key == "test":
                continue
            os_str = f"{os_str} {key}={value}"
        return os_str

    def command(self, args: Namespace) -> None:
        # Weird way to acquire current version
        package_cmd: list[Path | str] = [
            _repo_path / ".makepy" / "setup.py",
            "--no-history-dump",
            "--logging-level",
            "error",
            "conan-info",
            "package",
            "--name",
            "openocd",
        ]
        if args.assume_release:
            package_cmd.append("--assume-release")
        versioned_spec = run_shell(
            package_cmd, capture_output=True, loglevel=logging.DEBUG
        ).stdout.strip()
        revision = str(git.detect_repo(_repo_path).head.commit.hexsha).strip()
        ref = RevisionedRef.parse(f"{versioned_spec}#{revision}")

        conan = Conan(mp_conf=self.mp_conf)
        binaries = conan.search_binaries(ref)

        _logger.info(f"Found {len(binaries)} binary configurations:")
        for binary in binaries:
            url = conan.web_url(next(iter(binary.package_revisions)))
            _logger.info(self._format_info(binary))
            _logger.info(url)
            _logger.info("")


def _build_formatter_and_linter() -> tuple[FormatCommand, LintCommand]:
    makepy_files = [
        _repo_path / "make.py",
        _repo_path / ".makepy" / "_ocd_test_suite.py",
        _repo_path / ".makepy" / "setup.py",
    ]
    ocd_conanfile = _repo_path / "conanfile.py"
    ocd_testsuite_conanfile = _repo_path / "testsuite_conanfile.py"
    python_files = [*makepy_files, ocd_conanfile, ocd_testsuite_conanfile]

    format_cmd = FormatCommand(default_revision="origin/sc/main")
    # format_cmd.add_cmake_format() # Do not forget to add .cmake-format.py
    format_cmd.add_black(python_files)
    format_cmd.add_isort(python_files)

    lint_cmd = LintCommand(default_revision="origin/sc/main")
    lint_cmd.add_pylint(python_files)
    lint_cmd.add_mypy(makepy_files)
    lint_cmd.add_mypy([ocd_conanfile])
    lint_cmd.add_mypy([ocd_testsuite_conanfile])

    return format_cmd, lint_cmd


@_main_decorator()
def main() -> None:
    conductor = Conductor()

    conductor.add(GenericSuite())
    conductor.add(SyntacoreSuite())
    configs = ConanConfigs()
    for profile in [
        "mp_ubuntu18",
        "mp_ubuntu20",
        "mp_ubuntu22",
        "mp_centos7",
        "mp_rocky8",
        "mp_armhf",
        "makepy_sc_mingw",
    ]:
        configs.add(
            profile=profile,
            options={
                "source": ["internal", "syntacore"],
                "sanitize": ["disable"],
            },
        )

    conan = ConanSuite(
        name="openocd",
        start_version="2d580e9457771c9fe6bfd7b241a73ab65270d44e",
        start_semver="0.12.2",
        configs=configs,
        release_branch="sc/stable",
        recipe=_repo_path / "conanfile.py",
    )
    conan.add_package(
        name="openocd_testsuite",
        start_version="e5b87c3864349cb6e9ab0df471c628ea93ae7d86",
        start_semver="0.0.0",
        recipe=_repo_path / "testsuite" / "conanfile.py",
        configs=ConanConfigs().add_ubuntu20(),
    )

    conan = ConanSuite(
        name="openocd",
        start_version="2d580e9457771c9fe6bfd7b241a73ab65270d44e",
        start_semver="0.12.2",
        configs=configs,
        release_branch="sc/stable",
    )
    conan.add_package(
        name="openocd_testsuite",
        start_semver="0.0.1",
        recipe=_repo_path / "testsuite_conanfile.py",
        configs=ConanConfigs().add_ubuntu22(),
    )
    conductor.add(conan)
    conductor.add(_ConfigCommand())
    conductor.add(_BuildCommand())

    conductor.add(_OcdTestSuite())

    conductor.add(PrivilegedContainerHook())
    conductor.add(MakepyHintsHook(commands=["config", "build"]))
    conductor.add(ParallelHook(commands=["build"]))

    conductor.add(_PrintPackageURLs())

    conductor.add(HWRSSuite())

    format_cmd, lint_cmd = _build_formatter_and_linter()
    conductor.add(format_cmd)
    conductor.add(lint_cmd)

    conductor.process_shell_args()


if __name__ == "__main__":
    main()
