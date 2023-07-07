#!/usr/bin/env python3

# This script must only use Python Standard Library
import filecmp as _filecmp
import logging as _logging
import os as _os
import platform as _platform
import shutil as _shutil
import subprocess as _subprocess
import sys as _sys
from pathlib import Path as _Path

_logger = _logging.getLogger(__name__)


def run_in_venv(
    requirements_path: _Path,
    artifacts_path: _Path,
    setup_path: _Path | None = None,
    ssh_path: _Path | None = None,
    shell_args: list[str] | None = None,
) -> None:
    hostname = _platform.node()
    venv_path = artifacts_path / "venv" / hostname

    venv_requirements = venv_path / "requirements.txt"
    activate_path = venv_path / "bin" / "activate"

    env = _os.environ.copy()
    env["PATH"] = f"{venv_path}/bin:" + env["PATH"]
    env["VIRTUAL_ENV"] = str(venv_path)
    if ssh_path:
        env[
            "GIT_SSH_COMMAND"
        ] = f"ssh -o StrictHostKeyChecking=no -i {ssh_path}"

    if not activate_path.exists():
        _logger.info("Creating virtual environment...")
        _shutil.rmtree(venv_path, ignore_errors=True)
        _subprocess.run(
            [_sys.executable, "-m", "venv", venv_path],
            check=True,
            capture_output=False,
            text=True,
        )
    if not venv_requirements.exists() or not _filecmp.cmp(
        requirements_path, venv_requirements
    ):
        _logger.info("Updating virtual environment...")
        _subprocess.run(
            ["python3", "-m", "pip", "install", "-r", requirements_path],
            check=True,
            capture_output=False,
            text=True,
            env=env,
        )
        _shutil.copyfile(requirements_path, venv_requirements)

    setup: list[str]
    if isinstance(setup_path, _Path):
        setup = [str(setup_path)]
    else:
        setup = ["python3", "-m", "makepy"]
    shell_args = shell_args or _sys.argv
    shell_args = shell_args.copy()
    shell_args.pop(0)
    shell_args = setup + shell_args
    try:
        _subprocess.run(
            shell_args, capture_output=False, text=True, check=True, env=env
        )
    # pylint: disable=broad-except
    except (Exception, KeyboardInterrupt):
        # Intercept printing exception info:
        # It should already be printed by the called process at this moment.
        _sys.exit(1)


def _choose_alternative(*alts: _Path | None) -> _Path | None:
    for alternative in alts:
        if isinstance(alternative, _Path) and alternative.exists():
            return alternative
    return alts[-1]


def _main() -> None:
    default_makepy_path = _Path(__file__).parent / ".makepy"

    templated_requirements_path = _Path("{{ requirements_path }}")
    templated_artifacts_path = _Path("{{ artifacts_path }}")
    templated_setup_path = _Path("{{ setup_path }}")
    templated_ssh_path = _Path("{{ ssh_path }}")

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
    _logging.basicConfig(format="%(message)s", level=_logging.INFO)
    try:
        _main()
    # pylint: disable=broad-except
    except Exception as e:
        _logger.error(f"{type(e).__name__}: {e}")
        _sys.exit(1)
    except KeyboardInterrupt:
        _logger.error("==== Keyboard Interrupt ====")
        _sys.exit(1)
