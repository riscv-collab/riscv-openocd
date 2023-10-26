#!/usr/bin/env python3

import argparse as _argparse
import itertools as _itertools
import logging as _logging
import multiprocessing as _multiprocessing
import os as _os
import shutil as _shutil
import sys as _sys
from argparse import ArgumentParser as _ArgumentParser
from argparse import Namespace as _Namespace
from multiprocessing import cpu_count as _cpu_count
from pathlib import Path as _Path
from subprocess import CalledProcessError as _CalledProcessError
from typing import Any as _Any

import git as _git
import makepy.utils as _utils
import requests as _requests  # type: ignore
from makepy import ArgsHook as _ArgsHook
from makepy import Command as _Command
from makepy import Conductor as _Conductor
from makepy.generic import GenericSuite as _GenericSuite
from makepy.generic import ParallelHook as _ParallelHook
from makepy.generic import PrivilegedContainerHook as _PrivilegedContainerHook
from makepy.lint import FormatCommand as _FormatCommand
from makepy.lint import LintCommand as _LintCommand
from makepy.syntacore import ConanSuite as _ConanSuite
from makepy.syntacore import SyntacoreSuite as _SyntacoreSuite
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
        makepy = _repo_path / "make.py"
        output_folder = self._get_output_folder(args.build_path)
        install_cmd = [
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
        install_cmd.append(_repo_path)

        # TODO: should we remove all these "cwd" statements?
        _run_shell(["conan", "source", _repo_path], cwd=_repo_path)
        _run_shell(install_cmd, cwd=_repo_path)
        _run_shell(
            [
                makepy,
                "--no-history-dump",
                "config",
                "--build-path",
                args.build_path,
            ],
            cwd=_repo_path,
        )


class _ConfigCommand(_Command):
    def name(self) -> str:
        return "config"

    def help(self) -> str:
        return "CMake config."

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


class _PrepareDistribution(_Command):
    def name(self) -> str:
        return "distr-prep"

    def help(self) -> str:
        return 'prepares sources for "distribution" build'

    def amend_parser(self, parser: _ArgumentParser) -> None:
        parser.add_argument(
            "-r",
            "--release_string",
            dest="release_string",
            type=str,
            default="development-build",
            help="release string.",
        )

    def command(self, args: _Namespace) -> None:
        repo = _git.Repo.init(_repo_path)
        commit_hash = repo.head.commit.hexsha[:8]
        release_string = args.release_string.strip()
        if release_string == "":
            release_string = "development_build"
        release_string = f"{release_string}-g{commit_hash}"
        riscv_merge_base = _run_shell(
            ["git", "merge-base", "origin/riscv", "HEAD"],
            capture_output=True,
            cwd=_repo_path,
        ).stdout[0:7]
        version_info = f"riscv-upstream-{riscv_merge_base}-cs-{commit_hash}"
        with open(
            _repo_path / "__sc_version.txt", "w", encoding="utf-8"
        ) as version_file:
            version_file.write(f"{release_string}\n{version_info}")


def _build_formatter_and_linter() -> tuple[_FormatCommand, _LintCommand]:
    python_files = [
        _repo_path / "make.py",
        _repo_path / ".makepy" / "setup.py",
        _repo_path / ".makepy" / "support" / "utils" / "conan_the_deployer.py",
        _repo_path / "conanfile.py",
    ]

    format_cmd = _FormatCommand(default_revision="origin/sc/main")
    # format_cmd.add_cmake_format() # Do not forget to add .cmake-format.py
    format_cmd.add_black([*python_files])
    format_cmd.add_isort([*python_files])

    lint_cmd = _LintCommand(default_revision="origin/sc/main")
    lint_cmd.add_pylint([*python_files])
    lint_cmd.add_mypy([*python_files])
    lint_cmd.add_yamllint([_repo_path / ".gitlab-ci.yml"])

    return format_cmd, lint_cmd


@_main_decorator()
def _main() -> None:
    conductor = _Conductor()

    conductor.add(_GenericSuite())
    conductor.add(_SyntacoreSuite())
    conductor.add(
        _ConanSuite(
            name="openocd",
            start_version="cd481a97e9ae604880b8239679bf83534e83b381",
            start_semver="0.11.0",
        )
    )

    conductor.add(_JustConfigCommand())
    conductor.add(_ConfigCommand())
    conductor.add(_BuildCommand())
    conductor.add(_PrepareDistribution())

    conductor.add(_PrivilegedContainerHook())
    conductor.add(_BuildArgsHook())
    conductor.add(_ParallelHook(commands=["build"]))

    format_cmd, lint_cmd = _build_formatter_and_linter()
    conductor.add(format_cmd)
    conductor.add(lint_cmd)

    conductor.process_shell_args()


if __name__ == "__main__":
    _main()
