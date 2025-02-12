#!/usr/bin/env python3
from __future__ import annotations

import json
import logging
from argparse import ArgumentParser, Namespace
from pathlib import Path
from typing import TYPE_CHECKING

from hwrs import HWRSHook
from lgrw import ConanHelper, FpgaBoard
from makepy import Command, Hook, Suite
from makepy.syntacore import SyntacoreCredentialsHook

if TYPE_CHECKING:
    # pylint: disable=ungrouped-imports
    from lgrw._lgrunner import LabgridRunner

_logger = logging.getLogger()


class _OcdTestSuite(Suite):
    def __init__(self) -> None:
        commands = [
            "test-suite",
        ]

        self._handlers = [
            SyntacoreCredentialsHook(commands),
            HWRSHook(commands),
            _OcdTestSuiteCommand(),
        ]

    def handlers(self) -> list[Hook]:
        return self._handlers


class _OcdTestSuiteCommand(Command):
    def name(self) -> str:
        return "test-suite"

    def help(self) -> str:
        return "Install and run ocd_transferable_testsuite on FPGA host"

    def amend_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument("-b", "--build-path", required=True, type=Path)

        parser.add_argument(
            "--host",
            required=True,
            type=str,
            help="FPGA board host.",
        )

        parser.add_argument(
            "--install",
            action="store_true",
            help="Install ocd_transferable_testsuite before running tests.",
        )

        parser.add_argument(
            "--cleanup",
            action="store_true",
            help="Cleanup working dir on host.",
        )

        parser.add_argument(
            "--dry-run",
            action="store_true",
            help="Don't run anything on host, logging the execution.",
        )

        parser.add_argument(
            "--force",
            action="store_true",
            help="Ignore some errors. Use with caution.",
        )

        parser.add_argument(
            "--run-tool",
            type=str,
            nargs="*",
            help="Run tests of given tools.",
        )

        parser.add_argument(
            "--run-platform",
            default=None,
            type=str,
            help="Run tests for given platform.",
        )

        parser.add_argument(
            "--platforms",
            default="fpga_info/configurations.json",
            type=str,
            help="Path to json file with platforms",
        )

    def command(self, args: Namespace) -> None:
        # workspace_id will be used as directory, so lets cleanup it
        workspace_id = _safe_file_name(str(args.hwrs_id.id()))

        host = args.host
        local_build_path = args.build_path
        credentials = args.credentials

        # OpenOCD is *not* used by LGRW. It is passed only to provide correct
        # environment.
        openocd = (
            local_build_path
            / "install_testsuite"
            / "ocd_transferable_testsuite"
            / "openocd"
        )

        local_dir = local_build_path / "testing" / "lgrw"
        local_dir.mkdir(parents=True, exist_ok=True)

        remote_install = Path("/home/stand/.ci") / workspace_id

        board = FpgaBoard(
            host,
            credentials,
            openocd,
            local_build_path / "testing" / "lgrw-state",
            args.dry_run,
        )

        bitstream_path = None
        if args.run_platform:
            platform_desc = _get_platform_desc(
                args.platforms, args.run_platform
            )
            bitstream_path = Path(platform_desc["bitstream"])
            openocd_board = platform_desc["openocd_board"]

        with board.labgrid() as runner:
            try:
                if args.install:
                    _install_testsuite(
                        runner,
                        local_build_path / "install_testsuite",
                        remote_install,
                        args.force,
                    )
                if bitstream_path:
                    _flash_board(board, bitstream_path)
                if args.run_platform and args.run_tool:
                    _run_testsuite(
                        runner,
                        local_dir,
                        remote_install,
                        args.run_platform,
                        openocd_board,
                        args.run_tool,
                    )
            finally:
                if args.cleanup:
                    runner.run_shell(["rm", "-rf", remote_install])


def _safe_file_name(unsafe_name: str) -> str:
    safe_name = unsafe_name.replace("%2F", "-")  # the '/'

    # retain alphanumeric and '-' symbols only
    safe_name = "".join(filter(lambda c: c.isalnum() or c == "-", safe_name))

    return safe_name


def _install_testsuite(
    runner: LabgridRunner,
    local_install: Path,
    remote_install: Path,
    force: bool,
) -> None:
    testsuite_gz = "ocd_transferable_testsuite.tar.gz"
    local_testsuite_gz = local_install / testsuite_gz
    if (
        runner.run_shell(
            [
                "test",
                "-d",
                remote_install,
            ],
            check=False,
        ).returncode
        == 0
    ):
        cause = f"installation in {remote_install} already exists."
        if force:
            _logger.warning(cause)
        else:
            raise RuntimeError(
                f"Install failed: {cause}"
                "Please either:\n"
                f"- run `--cleanup` to remove it"
                f"- omit `--install` to reuse"
            )

    runner.run_shell(
        [
            "mkdir",
            "-p",
            remote_install,
        ],
    )
    remote_testsuite_gz = remote_install / testsuite_gz
    runner.send(
        local_testsuite_gz,
        remote_testsuite_gz,
    )
    runner.run_shell(
        [
            "tar",
            "-xzf",
            remote_testsuite_gz,
            "-C",
            remote_install,
        ]
    )


def _flash_board(board: FpgaBoard, bitstream_path: Path) -> None:
    conan = ConanHelper({})

    # Here the "cores" and "memory" used for normal LGRW usage (to run bare-metal programs and linux).
    # They are not used in our runs, but required in LGRW api, so can't be omitted.
    bitstream = conan.bitstream(
        {
            "path": bitstream_path,
            "cores": 1,
            "memory": "syntacore",
        }
    )
    board.flash(bitstream)


def _get_stand_info(runner: LabgridRunner, local_dir: Path) -> dict[str, str]:
    local_stand_info = local_dir / "tests.stand.info.json"
    remote_stand_info = Path(
        "/home/stand/.config/stand.info/tests.stand.info.json"
    )
    runner.receive(remote_stand_info, local_stand_info)

    with open(local_stand_info, encoding="utf-8") as json_file:
        stand_info: dict[str, str] = json.load(json_file)
    return stand_info


def _get_platform_desc(platforms: Path, platform_name: str) -> dict[str, str]:
    with open(platforms, encoding="utf-8") as json_file:
        platform_descriptions: dict[str, dict[str, str]] = json.load(json_file)

    if platform_name not in platform_descriptions:
        raise RuntimeError(
            f"Unknown platform: {platform_name}, "
            f"available: {platform_descriptions.keys()}"
        )

    return platform_descriptions[platform_name]


def _run_testsuite(
    runner: LabgridRunner,
    local_dir: Path,
    remote_dir: Path,
    platform_name: str,
    openocd_board: str,
    tools: list[str],
) -> None:
    stand_info = _get_stand_info(runner, local_dir)

    failures = [
        not _run_tool_tests(
            runner,
            remote_dir,
            stand_info,
            platform_name,
            openocd_board,
            tool,
        )
        for tool in tools
    ]

    log_tar_gz = f"{platform_name}.tar.gz"
    remote_log_tar_gz = remote_dir / log_tar_gz
    runner.run_shell(
        [
            "tar",
            "-C",
            remote_dir,
            "-czf",
            remote_log_tar_gz,
            platform_name,
        ],
    )
    runner.receive(
        remote_log_tar_gz,
        local_dir / log_tar_gz,
    )
    if any(failures):
        raise RuntimeError("Tests failed.")


def _run_tool_tests(
    runner: LabgridRunner,
    remote_dir: Path,
    stand_info: dict[str, str],
    platform_name: str,
    openocd_board: str,
    tool: str,
) -> bool:
    run_base_dir = remote_dir / platform_name / tool
    work_dir = run_base_dir / "runs"
    summary_dir = run_base_dir / "SUMMARY"
    remote_testsuite = remote_dir / "ocd_transferable_testsuite"
    command: list[str | Path] = []
    command += ["mkdir -p", work_dir, "&&"]
    command += ["mkdir -p", summary_dir, "&&"]
    command += ["cd", work_dir, "&&"]
    command += [
        f'DEJAGNU="{remote_testsuite}/site.exp"',
        f'OPENOCD_DEBUG_ADAPTER_SERIAL="{stand_info["adapter_serial"]}"',
        f'OPENOCD_DEBUG_ADAPTER_CONFIG="{stand_info["adapter_config"]}"',
        f'OPENOCD_DEBUG_ADAPTER_SPEED="{stand_info["adapter_speed"]}"',
        f"{remote_testsuite}/dejagnu/bin/runtest",
        "--tool",
        tool,
        "--srcdir",
        f"{remote_testsuite}/acceptance_tests/testsuite",
        "--target_board",
        openocd_board,
        f"--outdir={summary_dir}",
    ]
    return bool(runner.run_shell(command, check=False).returncode == 0)
