#!/usr/bin/env python3

# This script must be compatible with Python 3.8+
from __future__ import annotations

import filecmp
import logging
import os
import platform
import re
import shutil
import subprocess
import sys
from getpass import getuser
from pathlib import Path

_logger = logging.getLogger(__name__)


def _setup_pip_venv(root: Path) -> tuple[Path, dict[str, str]]:
    hostname = platform.node()
    artifacts_path = root / ".makepy" / "artifacts"
    requirements_path = root / ".makepy" / "requirements.txt"
    venv_path = artifacts_path / "venv" / hostname
    venv_requirements = venv_path / "requirements.txt"
    env = os.environ.copy()

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

    if not venv_executable.exists():
        _logger.info("Creating virtual environment...")
        shutil.rmtree(venv_path, ignore_errors=True)
        if sys.version_info[0] < 3 or (
            sys.version_info[0] == 3 and sys.version_info[1] < 10
        ):
            _logger.info(
                "At least Python 3.10 is required, trying to find one..."
            )
            python_executable = None
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
            check=False,
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
            cwd=root,
        )
        shutil.copyfile(requirements_path, venv_requirements)
    return venv_executable, env


def _uv_executable() -> Path | None:
    uv_executable: Path | str | None

    uv_executable = os.environ.get("UV")
    if uv_executable is not None:
        return Path(uv_executable)

    uv_executable = Path.home() / ".cargo" / "bin" / "uv"
    if uv_executable.exists():
        return uv_executable

    uv_executable = shutil.which("uv")
    if uv_executable is None:
        return None
    return Path(uv_executable)


def _uv_python_version(root: Path) -> str:
    default = ">=3.10"
    pyproject_path = root / "pyproject.toml"
    if not pyproject_path.exists():
        return default
    pyproject = pyproject_path.read_text(encoding="utf-8")
    if not (match := re.search(r"requires-python\s*=(.*)", pyproject)):
        return default
    version = match.group(1).strip("\"' ")
    if not version:
        return default
    return version


def _setup_legacy_uv_venv(root: Path) -> tuple[Path, dict[str, str]]:
    hostname = platform.node()
    artifacts_path = root / ".makepy" / "artifacts"
    requirements_path = root / ".makepy" / "requirements.txt"
    venv_path = artifacts_path / "venv" / f"uv-{hostname}"
    venv_requirements = venv_path / "requirements.txt"
    env = os.environ.copy()

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
    env["UV_PYTHON"] = str(venv_executable)

    uv_executable = _uv_executable()
    assert uv_executable is not None, "uv executable not found!"
    python_version = _uv_python_version(root)

    if not venv_executable.exists():
        _logger.info("Creating virtual environment...")
        shutil.rmtree(venv_path, ignore_errors=True)

        subprocess.run(
            [
                uv_executable,
                "venv",
                "--quiet",
                "--preview",
                "--python",
                python_version,
                venv_path,
            ],
            check=True,
            capture_output=False,
            text=True,
        )
        if not venv_executable.exists():
            raise RuntimeError("Failed to create virtual environment.")
    if not venv_requirements.exists() or not filecmp.cmp(
        requirements_path, venv_requirements
    ):
        _logger.info("Updating virtual environment...")
        subprocess.run(
            [
                uv_executable,
                "pip",
                "install",
                "-r",
                requirements_path,
                "--quiet",
            ],
            check=True,
            capture_output=False,
            text=True,
            env=env,
            cwd=root,
        )
        shutil.copyfile(requirements_path, venv_requirements)
    return venv_executable, env


def _setup_uv_venv(root: Path) -> tuple[Path, dict[str, str]]:
    venv_path = root / ".venv"
    lock = root / "uv.lock"
    venv_lock = venv_path / "uv.lock"
    env = os.environ.copy()

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
    env["UV_PYTHON"] = str(venv_executable)

    uv_executable = _uv_executable()
    assert uv_executable is not None, "uv executable not found!"

    if (
        not venv_executable.exists()
        or not venv_lock.exists()
        or not filecmp.cmp(lock, venv_lock)
    ):
        _logger.info("Synchronizing virtual environment...")

        cmd: list[str | Path] = [uv_executable, "sync", "--all-extras"]
        if "[tool.uv.workspace]" in (root / "pyproject.toml").read_text(
            encoding="utf-8"
        ):
            cmd.append("--all-packages")

        subprocess.run(
            cmd, check=True, capture_output=False, text=True, cwd=root
        )
        if not venv_executable.exists():
            raise RuntimeError("Failed to create virtual environment.")

        shutil.copyfile(lock, venv_lock)
    return venv_executable, env


def _detect_if_legacy_venv_needed(root: Path) -> bool:
    requirements_path = root / ".makepy" / "requirements.txt"
    if not requirements_path.exists():
        return False
    if not requirements_path.read_text(encoding="utf-8").strip():
        return False
    pyproject_path = root / "pyproject.toml"
    if not pyproject_path.exists():
        raise RuntimeError(
            "Cannot find pyproject.toml or .makepy/requirements.txt files!"
        )

    pyproject = pyproject_path.read_text(encoding="utf-8")
    return not bool(
        re.search(r"legacy_venv\s*=\s*[\"']?false[\"']?", pyproject.lower())
    )


def _setup_venv(root: Path) -> tuple[Path, dict[str, str]]:
    if not _detect_if_legacy_venv_needed(root):
        return _setup_uv_venv(root)

    # Both uv and pip can be set up either with configuration files or environment variables.
    # However, we are moving towards dynamic configuration with uv.
    # For this reason:
    # 1. If uv executable is found AND UV_* vars is present, we use uv.
    # 2. Otherwise, use old-style pip venv setup.
    uv_executable = _uv_executable()
    if uv_executable is not None and os.getenv("UV_INDEX_URL") is not None:
        return _setup_legacy_uv_venv(root)

    return _setup_pip_venv(root)


def _run_in_venv(root: Path, shell_args: list[str]) -> None:
    setup_path = root / ".makepy" / "setup.py"
    venv_exe, env = _setup_venv(root)

    setup = [str(venv_exe), str(setup_path)]
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


def _check_candidate(path: Path) -> Path | None:
    if not (path / ".makepy").exists():
        return None
    return path


def _root() -> Path:
    the_script = Path(__file__)
    if the_script.name == "mpy":
        # Emulate mpy behavior
        current_path = Path.cwd()
        os.environ["MPY_RUNNER"] = str(the_script)
    else:
        current_path = the_script.parent  # Default make.py behavior

    while (candidate := _check_candidate(current_path)) is None:
        if (
            os.name != "nt"
            and (current_path.owner() != current_path.parent.owner())
            and (current_path.parent.owner() != getuser())
        ):
            break
        if current_path == current_path.parent:
            break
        current_path = current_path.parent

    if candidate is None:
        raise RuntimeError("Cannot find a .makepy directory!")
    return candidate


def _main() -> None:
    _run_in_venv(_root(), shell_args=sys.argv.copy())


if __name__ == "__main__":
    # [CHECK FOR SYSTEM-WIDE make.py]
    if Path(__file__).name != "mpy" and shutil.which("mpy") is not None:
        args = sys.argv.copy()
        args[0] = "mpy"
        sys.exit(subprocess.run(args, check=False).returncode)
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
