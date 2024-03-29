#!/usr/bin/env python3
from __future__ import annotations

import json
import logging as _logging
from argparse import ArgumentParser as _ArgumentParser
from argparse import Namespace as _Namespace
from pathlib import Path as _Path

from hwrs import HWRSHook as _HWRSHook
from lgrw import FpgaBoard as _FpgaBoard
from lgrw._conan_support import ConanHelper
from makepy import Command as _Command
from makepy import Hook as _Hook
from makepy import Suite as _Suite
from makepy.syntacore import (
    SyntacoreCredentialsHook as _SyntacoreCredentialsHook,
)

_logger = _logging.getLogger()


class _OcdTestSuite(_Suite):
    def __init__(self) -> None:
        commands = [
            "test-suite",
        ]

        self._handlers = [
            _SyntacoreCredentialsHook(commands),
            _HWRSHook(commands),
            _OcdTestSuiteCommand(),
        ]

    def handlers(self) -> list[_Hook]:
        return self._handlers


class _OcdTestSuiteCommand(_Command):
    def name(self) -> str:
        return "test-suite"

    def help(self) -> str:
        return "Install and run ocd_transferable_testsuite on FPGA host"

    def amend_parser(self, parser: _ArgumentParser) -> None:
        parser.add_argument("-b", "--build-path", required=True, type=_Path)

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
            "--run",
            type=str,
            help="Run tests for given platform",
        )

        parser.add_argument(
            "--platforms",
            default="fpga_info/configurations.json",
            type=str,
            help="Path to json file with platforms",
        )

    def command(self, args: _Namespace) -> None:
        # workspace_id will be used as directory, so lets cleanup it
        workspace_id = _safe_file_name(str(args.hwrs_id.id()))

        host = args.host
        local_build_path = args.build_path
        credentials = args.credentials

        openocd = (
            local_build_path
            / "install_testsuite"
            / "ocd_transferable_testsuite"
            / "openocd"
        )

        local_testsuite_gz = (
            local_build_path
            / "install_testsuite"
            / "ocd_transferable_testsuite.tar.gz"
        )
        local_dir = local_build_path / "testing" / "lgrw"
        local_dir.mkdir(parents=True, exist_ok=True)

        remote_stand_info = _Path(
            "/home/stand/.config/stand.info/tests.stand.info.json"
        )
        remote_dir = _Path("/home/stand/.ci") / workspace_id
        remote_testsuite_gz = remote_dir / "ocd_transferable_testsuite.tar.gz"
        remote_testsuite = remote_dir / "ocd_transferable_testsuite"

        board = _FpgaBoard(
            host,
            credentials,
            openocd,
            local_build_path / "testing" / "lgrw-state",
        )

        try:
            if args.install:
                with board.labgrid() as runner:
                    if (
                        runner.run_shell(
                            [
                                "mkdir",
                                "-p",
                                remote_dir,
                            ],
                            check=False,
                        ).returncode
                        != 0
                    ):
                        raise RuntimeError(
                            f"Install failed: installation for workspace {workspace_id} already exists. "
                            f"Please either:\n"
                            f"- run `--cleanup` to remove it"
                            f"- omit `--install` to reuse"
                        )

                    runner.send(
                        local_testsuite_gz,
                        remote_testsuite_gz,
                        capture_output=True,
                    )
                    runner.run_shell(
                        [
                            "tar",
                            "-xzf",
                            remote_testsuite_gz,
                            "-C",
                            remote_dir,
                        ]
                    )

            if args.run:
                configuration_name = str(args.run)
                _logger.info("Running %s", configuration_name)

                with open(args.platforms, encoding="utf-8") as json_file:
                    fpga_configurations = json.load(json_file)

                if configuration_name not in fpga_configurations:
                    raise RuntimeError(
                        f"Unknown fpga configuration: {configuration_name}, "
                        f"available: {fpga_configurations.keys()}"
                    )

                configuration = fpga_configurations[configuration_name]
                openocd_board = configuration["openocd_board"]
                bitstream_path = configuration["bitstream"]

                _logger.info("Configuration:\n%s", json.dumps(configuration))

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

                with board.labgrid() as runner:
                    local_stand_info = local_dir / "tests.stand.info.json"
                    runner.receive(remote_stand_info, local_stand_info)

                    with open(local_stand_info, encoding="utf-8") as json_file:
                        stand_info = json.load(json_file)

                    adapter_serial = stand_info["adapter_serial"]
                    adapter_config = stand_info["adapter_config"]
                    adapter_speed = stand_info["adapter_speed"]

                    work_dir = remote_dir / configuration_name
                    summary_dir = work_dir / "SUMMARY"

                    command: list[str | _Path] = []
                    command += ["mkdir", work_dir, "&&"]
                    command += ["mkdir", summary_dir, "&&"]
                    command += ["cd", remote_dir / configuration_name, "&&"]
                    command += [
                        f'DEJAGNU="{remote_testsuite}/site.exp"',
                        f'OPENOCD_DEBUG_ADAPTER_SERIAL="{adapter_serial}"',
                        f'OPENOCD_DEBUG_ADAPTER_CONFIG="{adapter_config}"',
                        f'OPENOCD_DEBUG_ADAPTER_SPEED="{adapter_speed}"',
                        f"{remote_testsuite}/dejagnu/bin/runtest",
                        "--tool",
                        "ocd",
                        "--srcdir",
                        f"{remote_testsuite}/acceptance_tests/testsuite",
                        "--target_board",
                        openocd_board,
                        f"--outdir={summary_dir}",
                    ]

                    if runner.run_shell(command, check=False).returncode != 0:
                        remote_log_tar_gz = (
                            remote_dir / f"{configuration_name}.tar.gz"
                        )
                        if (
                            runner.run_shell(
                                [
                                    "tar",
                                    "-C",
                                    remote_dir,
                                    "-czf",
                                    remote_log_tar_gz,
                                    configuration_name,
                                ],
                                check=False,
                            ).returncode
                            == 0
                        ):
                            runner.receive(
                                remote_log_tar_gz,
                                local_dir / f"{configuration_name}.tar.gz",
                            )

                        raise RuntimeError("Tests failed.")
        finally:
            if args.cleanup:
                runner.run_shell(["rm", "-rf", remote_dir])


def _safe_file_name(unsafe_name: str) -> str:
    safe_name = unsafe_name.replace("%2F", "-")  # the '/'

    # retain alphanumeric and '-' symbols only
    safe_name = "".join(filter(lambda c: c.isalnum() or c == "-", safe_name))

    return safe_name
