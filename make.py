#!/usr/bin/env python3

# pylint: disable=too-many-branches
# pylint: disable=too-many-statements
# This script must only use Python Standard Library
from __future__ import annotations

import filecmp
import logging
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

_logger = logging.getLogger(__name__)


def run_in_venv(
    requirements_path: Path,
    artifacts_path: Path,
    setup_path: Path | None = None,
    ssh_path: Path | None = None,
    shell_args: list[str] | None = None,
) -> None:
    hostname = platform.node()
    venv_path = artifacts_path / "venv" / hostname
    env = os.environ.copy()

    venv_requirements = venv_path / "requirements.txt"
    if os.name == "posix":
        bin_path = venv_path / "bin"
        venv_executable = bin_path / "python"
    elif os.name == "nt":
        bin_path = venv_path / "Scripts"
        venv_executable = bin_path / "python.exe"
    else:
        raise RuntimeError(f"Unsupported OS: {os.name}")

    env["PATH"] = os.pathsep.join((str(bin_path), env["PATH"]))
    env["VIRTUAL_ENV"] = str(venv_path)
    if ssh_path:
        env["GIT_SSH_COMMAND"] = (
            f"ssh -o StrictHostKeyChecking=no -i {ssh_path}"
        )

    if not venv_executable.exists():
        _logger.info("Creating virtual environment...")
        shutil.rmtree(venv_path, ignore_errors=True)
        if sys.version_info[0] < 3 or (
            sys.version_info[0] == 3 and sys.version_info[1] < 10
        ):
            _logger.info(
                "At least Python 3.10 is required, trying to find one..."
            )
            for subversion in range(10, 20):
                python_executable = shutil.which(f"python3.{subversion}")
                if python_executable is not None:
                    break
            if python_executable is None:
                raise RuntimeError("Failed to find Python version >= 3.10.")
        else:
            python_executable = sys.executable

        subprocess.run(
            [python_executable, "-m", "venv", venv_path],
            check=True,
            capture_output=False,
            text=True,
        )
        if not venv_executable.exists():
            raise RuntimeError(
                "Failed to create virtual environment. Did you install pip?"
            )
    if not venv_requirements.exists() or not filecmp.cmp(
        requirements_path, venv_requirements
    ):
        _logger.info("Updating virtual environment...")
        subprocess.run(
            [venv_executable, "-m", "pip", "install", "-r", requirements_path],
            check=True,
            capture_output=False,
            text=True,
            env=env,
        )
        shutil.copyfile(requirements_path, venv_requirements)

    setup: list[str]
    if isinstance(setup_path, Path):
        setup = [str(venv_executable), str(setup_path)]
    else:
        setup = [str(venv_executable), "-m", "makepy"]
    shell_args = shell_args or sys.argv
    shell_args = shell_args.copy()
    shell_args.pop(0)
    shell_args = setup + shell_args
    try:
        subprocess.run(
            shell_args, capture_output=False, text=True, check=True, env=env
        )
    # Intercept printing exception info:
    # It should already be printed by the called process at this moment.
    except subprocess.CalledProcessError as exc:
        assert exc.returncode != 0
        sys.exit(exc.returncode)
    except KeyboardInterrupt:
        sys.exit(2)


def _choose_alternative(*alts: Path | None) -> Path | None:
    for alternative in alts:
        if isinstance(alternative, Path) and alternative.exists():
            return alternative
    return alts[-1]


def _main() -> None:
    default_makepy_path = Path(__file__).parent / ".makepy"

    templated_requirements_path = Path("{{ requirements_path }}")
    templated_artifacts_path = Path("{{ artifacts_path }}")
    templated_setup_path = Path("{{ setup_path }}")
    templated_ssh_path = Path("{{ ssh_path }}")

    requirements_path = _choose_alternative(
        templated_requirements_path, default_makepy_path / "requirements.txt"
    )
    assert requirements_path is not None
    artifacts_path = _choose_alternative(
        templated_artifacts_path, default_makepy_path / "artifacts"
    )
    assert artifacts_path is not None
    setup_path = _choose_alternative(
        templated_setup_path, default_makepy_path / "setup.py", None
    )
    ssh_path = _choose_alternative(templated_ssh_path, None)

    run_in_venv(
        requirements_path=requirements_path,
        artifacts_path=artifacts_path,
        setup_path=setup_path,
        ssh_path=ssh_path,
    )


if __name__ == "__main__":
    # [CHECK FOR SYSTEM-WIDE make.py]
    mpy: str | Path | None = shutil.which("mpy")
    # YCAT-42451 hot fix (temporary)
    if mpy is None:
        mpy = Path.home() / ".local" / "bin" / "mpy"
        if not mpy.is_file():
            mpy = None  # pylint: disable=C0103
    if mpy is not None:
        args = sys.argv.copy()
        args[0] = str(mpy)
        sys.exit(
            subprocess.run(
                args, check=False, cwd=Path(__file__).parent
            ).returncode
        )
    # [/CHECK FOR SYSTEM-WIDE make.py]

    logging.basicConfig(format="%(message)s", level=logging.INFO)
    try:
        _main()
    # pylint: disable=broad-except
    except Exception as e:
        _logger.error(f"{type(e).__name__}: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        _logger.error("==== Keyboard Interrupt ====")
        sys.exit(2)
