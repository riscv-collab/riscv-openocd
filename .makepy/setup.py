#!/usr/bin/env python3

import itertools as _itertools
import logging as _logging
import shutil as _shutil
from argparse import ArgumentParser as _ArgumentParser
from argparse import Namespace as _Namespace
from pathlib import Path as _Path

from _ocd_test_suite import _OcdTestSuite
from hwrs import HWRSSuite as _HWRSSuite
from makepy import ArgsHook as _ArgsHook
from makepy import Command as _Command
from makepy import Conductor as _Conductor
from makepy.conan import BinaryPackage, Conan, RevisionedRef
from makepy.generic import GenericSuite as _GenericSuite
from makepy.generic import ParallelHook as _ParallelHook
from makepy.generic import PrivilegedContainerHook as _PrivilegedContainerHook
from makepy.lint import FormatCommand as _FormatCommand
from makepy.lint import LintCommand as _LintCommand
from makepy.syntacore import ConanConfigs as _ConanConfigs
from makepy.syntacore import ConanSuite as _ConanSuite
from makepy.syntacore import SyntacoreSuite as _SyntacoreSuite
from makepy.utils import git
from makepy.utils import main as _main_decorator
from makepy.utils import run_shell as _run_shell

_logger = _logging.getLogger()
_repo_path = _Path(__file__).parent.parent


class _BuildArgsHook(_ArgsHook):
    def amend_commands(self) -> list[str]:
        return ["config", "build", "just-config"]

    def amend_parser(self, parser: _ArgumentParser) -> None:
        parser.add_argument(
            "-b",
            "--build-path",
            required=True,
            type=_Path,
            help="Build directory.",
        )

    def hook(self, args: _Namespace) -> None:
        args.install_path = (args.build_path / "install").absolute()


class _JustConfigCommand(_Command):
    def name(self) -> str:
        return "just-config"

    def help(self) -> str:
        return "Just config the project!"

    def amend_parser(self, parser: _ArgumentParser) -> None:
        conan_help = "See './make.py conan install --help'."
        parser.add_argument(
            "--build", type=str, default="never", help=conan_help
        )
        parser.add_argument(
            "-pr",
            "--profile",
            "-pr:h",
            "--profile:host",
            dest="host_profile",
            type=str,
            default="default",
            help=conan_help,
        )
        parser.add_argument(
            "-pr:b",
            "--profile:build",
            dest="build_profile",
            type=str,
            default="default",
            help=conan_help,
        )
        parser.add_argument(
            "-o",
            "--options",
            "-o:h",
            "--options:host",
            dest="host_options",
            type=str,
            default=[],
            action="append",
            help=conan_help,
        )
        parser.add_argument(
            "-o:b",
            "--options:build",
            dest="build_options",
            type=str,
            default=[],
            action="append",
            help=conan_help,
        )
        parser.add_argument(
            "-s",
            "--settings",
            "-s:h",
            "--settings:host",
            dest="host_settings",
            type=str,
            default=[],
            action="append",
            help=conan_help,
        )
        parser.add_argument(
            "-s:b",
            "--settings:build",
            dest="build_settings",
            type=str,
            default=[],
            action="append",
            help=conan_help,
        )

        parser.add_argument(
            "--tests-options",
            dest="tests_options",
            type=str,
            default=[],
            action="append",
            help="additional parameters relevant for testing",
        )
        parser.add_argument(
            "--sanitize-level",
            dest="sanitize_level",
            type=str,
            default=None,
            help="ASan/UBSan sanitizaion flags",
        )

    def _get_output_folder(self, build_path: _Path) -> _Path:
        msg = (
            "Conan will always generate files into build/(Debug|Release), "
            "so your build path should be in the format <any_mangle>/build/(Debug|Release). "
            "You can change this behavior in conanfile.py."
        )
        if build_path.name not in ["Debug", "Release"]:
            raise ValueError(msg)
        if build_path.parent.name != "build":
            raise ValueError(msg)
        return build_path.parent.parent

    def command(self, args: _Namespace) -> None:
        _shutil.rmtree(_repo_path / "external_sources", ignore_errors=True)
        makepy = [
            _repo_path / "make.py",
            "--no-history-dump",
            "--logging-level",
            args.logging_level.lower(),
        ]
        output_folder = self._get_output_folder(args.build_path)
        install_cmd = [
            *makepy,
            "conan",
            "install",
            "--profile:host",
            args.host_profile,
            "--profile:build",
            args.build_profile,
            "--build",
            args.build,
            "--output-folder",
            output_folder,
        ]
        install_cmd.extend(
            _itertools.chain.from_iterable(
                ["--settings:host", setting] for setting in args.host_settings
            )
        )
        install_cmd.extend(
            _itertools.chain.from_iterable(
                ["--settings:build", setting] for setting in args.build_settings
            )
        )
        install_cmd.extend(
            _itertools.chain.from_iterable(
                ["--options:host", setting] for setting in args.host_options
            )
        )
        install_cmd.extend(
            _itertools.chain.from_iterable(
                ["--options:build", setting] for setting in args.build_options
            )
        )

        # TODO: should we remove all these "cwd" statements?
        _run_shell([*makepy, "conan", "source"], cwd=_repo_path)
        _run_shell(install_cmd, cwd=_repo_path)
        config_cmd = [
            *makepy,
            "config",
            "--build-path",
            args.build_path,
        ]

        config_cmd.extend(
            _itertools.chain.from_iterable(
                [f"--{option}"] for option in args.tests_options
            )
        )
        if args.sanitize_level is not None:
            config_cmd.extend([f"--sanitize-level={args.sanitize_level}"])

        _run_shell(config_cmd, cwd=_repo_path)


class _ConfigCommand(_Command):
    def name(self) -> str:
        return "config"

    def help(self) -> str:
        return "CMake config."

    def amend_parser(self, parser: _ArgumentParser) -> None:
        parser.add_argument(
            "--tests-adapter-info",
            dest="tests_adapter_info",
            type=str,
            default=None,
            help="json file with debug adapter properties",
        )
        parser.add_argument(
            "--tests-valgrid-path",
            dest="tests_valgrind_path",
            type=str,
            default=None,
            help="run OpenOCD under valgrid when running tests",
        )
        parser.add_argument(
            "--sanitize-level",
            dest="sanitize_level",
            choices=[None, "Enabled", "Strict"],
            type=str,
            default=None,
            help="controls ASan/UBSan behavior if enabled",
        )

    def command(self, args: _Namespace) -> None:
        _shutil.rmtree(_repo_path / "build-aux", ignore_errors=True)
        # NOTE: we expect OpenOCD submodules to be initialized at this point
        _run_shell(["./bootstrap", "nosubmodule"], cwd=_repo_path)
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
            cmd.extend(
                [f"-DOPENOCD_TESTS_VALGRIND_PATH={args.tests_valgrind_path}"]
            )

        if args.sanitize_level is None:
            pass
        elif args.sanitize_level == "Enabled":
            cmd.extend(["-DSC_OPENOCD_ENABLE_SANITIZERS=ON"])
        elif args.sanitize_level == "Strict":
            cmd.extend(["-DSC_OPENOCD_ENABLE_SANITIZERS=ON"])
            cmd.extend(["-DSC_OPENOCD_STRICT_SANITIZERS=ON"])
        else:
            raise ValueError(
                f"unknown sanitization level {args.sanitize_level}"
            )
        _run_shell(cmd)


class _BuildCommand(_Command):
    def name(self) -> str:
        return "build"

    def help(self) -> str:
        return "CMake build."

    def amend_parser(self, parser: _ArgumentParser) -> None:
        parser.add_argument(
            "-t",
            "--target",
            dest="cmake_target",
            type=str,
            default="openocd",
            help="Build target for CMake.",
        )

    def command(self, args: _Namespace) -> None:
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
        _run_shell(cmd)


class _PrintPackageURLs(_Command):
    """
    WARNING: NEVER CALL THIS FROM THE CONAN RECIPE
    """

    def name(self) -> str:
        return "print-package-urls"

    def help(self) -> str:
        return "Search for package URLs in the Conan registry."

    def amend_parser(self, parser: _ArgumentParser) -> None:
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

    def command(self, args: _Namespace) -> None:
        # Weird way to acquire current version
        package_cmd: list[_Path | str] = [
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
        versioned_spec = _run_shell(
            package_cmd, capture_output=True, loglevel=_logging.DEBUG
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


def _build_formatter_and_linter() -> tuple[_FormatCommand, _LintCommand]:
    python_files = [
        _repo_path / "make.py",
        _repo_path / ".makepy" / "_ocd_test_suite.py",
        _repo_path / ".makepy" / "setup.py",
        _repo_path / "conanfile.py",
    ]

    format_cmd = _FormatCommand(default_revision="origin/sc/main")
    # format_cmd.add_cmake_format() # Do not forget to add .cmake-format.py
    format_cmd.add_black([*python_files])
    format_cmd.add_isort([*python_files])

    lint_cmd = _LintCommand(default_revision="origin/sc/main")
    lint_cmd.add_pylint([*python_files])
    lint_cmd.add_mypy([*python_files])

    return format_cmd, lint_cmd


@_main_decorator()
def _main() -> None:
    conductor = _Conductor()

    conductor.add(_GenericSuite())
    conductor.add(_SyntacoreSuite())
    configs = _ConanConfigs()
    configs.add(profile="mp_armhf", options={"elct_support": True})
    configs.add_windows(options={"elct_support": [False, True]})
    configs.add_ubuntu18(options={"elct_support": [False, True]})
    configs.add_centos7(options={"elct_support": [False, True]})
    configs.add_ubuntu20(
        options={"elct_support": [False, True], "test": [False, True]}
    )
    configs.add_ubuntu22(
        options={"elct_support": [False, True], "test": [False, True]}
    )
    configs.add_rocky8(
        options={"elct_support": [False, True], "test": [False, True]}
    )

    conductor.add(
        _ConanSuite(
            name="openocd",
            start_version="cd481a97e9ae604880b8239679bf83534e83b381",
            start_semver="0.11.0",
            configs=configs,
            release_branch="sc/stable",
        )
    )

    conductor.add(_JustConfigCommand())
    conductor.add(_ConfigCommand())
    conductor.add(_BuildCommand())

    conductor.add(_OcdTestSuite())

    conductor.add(_PrivilegedContainerHook())
    conductor.add(_BuildArgsHook())
    conductor.add(_ParallelHook(commands=["build"]))

    conductor.add(_PrintPackageURLs())

    conductor.add(_HWRSSuite())

    format_cmd, lint_cmd = _build_formatter_and_linter()
    conductor.add(format_cmd)
    conductor.add(lint_cmd)

    conductor.process_shell_args()


if __name__ == "__main__":
    _main()
