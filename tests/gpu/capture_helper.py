from __future__ import annotations

import shlex
import subprocess
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol


@dataclass(frozen=True)
class CaptureMetadata:
    capture_path: Path
    invoked_command: str
    stdout: str
    stderr: str
    exit_code: int


class SubprocessRunner(Protocol):
    def __call__(
        self,
        args: Sequence[str | Path],
        *,
        cwd: Path | None = None,
        env: Mapping[str, str] | None = None,
        timeout_seconds: int | None = None,
        artifact_dir: Path | None = None,
        check: bool = True,
    ) -> subprocess.CompletedProcess[str]: ...


def _stringify(values: Sequence[str | Path]) -> list[str]:
    return [str(value) for value in values]


def _command_text(command: Sequence[str]) -> str:
    return shlex.join(command)


def format_capture_error(
    *,
    message: str,
    capture_path: Path,
    command: str,
    exit_code: int | None = None,
    stdout: str = "",
    stderr: str = "",
) -> str:
    lines = [
        message,
        f"Capture path: {capture_path}",
        f"Command: {command}",
    ]
    if exit_code is not None:
        lines.append(f"Exit code: {exit_code}")
    if stdout:
        lines.append(f"stdout:\n{stdout}")
    if stderr:
        lines.append(f"stderr:\n{stderr}")
    return "\n".join(lines)


def resolve_capture_path(
    artifact_dir: Path, capture_name: str | Path = "capture.rdc"
) -> Path:
    capture_path = Path(capture_name)
    if not capture_path.is_absolute():
        capture_path = artifact_dir / capture_path
    if capture_path.suffix != ".rdc":
        raise ValueError(f"Capture output must end with '.rdc': {capture_path}")
    return capture_path


def assert_capture_exists(
    capture_path: Path,
    command: str,
    *,
    stdout: str = "",
    stderr: str = "",
    exit_code: int | None = None,
) -> Path:
    if capture_path.suffix != ".rdc":
        raise RuntimeError(
            format_capture_error(
                message="Capture path must end with '.rdc'.",
                capture_path=capture_path,
                command=command,
                exit_code=exit_code,
                stdout=stdout,
                stderr=stderr,
            )
        )

    if not capture_path.is_file():
        raise RuntimeError(
            format_capture_error(
                message="Capture file was not produced.",
                capture_path=capture_path,
                command=command,
                exit_code=exit_code,
                stdout=stdout,
                stderr=stderr,
            )
        )

    return capture_path


def build_headless_goggles_command(
    *,
    goggles_binary: Path,
    target_binary: Path,
    goggles_args: Sequence[str | Path] = (),
    target_args: Sequence[str | Path] = (),
) -> list[str]:
    return [
        str(goggles_binary),
        "--headless",
        *_stringify(goggles_args),
        "--",
        str(target_binary),
        *_stringify(target_args),
    ]


def build_rdc_capture_command(
    *,
    capture_path: Path,
    app_command: Sequence[str],
    rdc_binary: str = "rdc",
    capture_args: Sequence[str | Path] = (),
) -> list[str]:
    return [
        rdc_binary,
        "capture",
        "--output",
        str(capture_path),
        *_stringify(capture_args),
        "--",
        *app_command,
    ]


def run_headless_capture(
    *,
    run_subprocess: SubprocessRunner,
    artifact_dir: Path,
    goggles_binary: Path,
    target_binary: Path,
    timeout_seconds: int,
    capture_name: str | Path = "capture.rdc",
    rdc_binary: str = "rdc",
    goggles_args: Sequence[str | Path] = (),
    target_args: Sequence[str | Path] = (),
    capture_args: Sequence[str | Path] = (),
    cwd: Path | None = None,
    env: Mapping[str, str] | None = None,
) -> CaptureMetadata:
    capture_path = resolve_capture_path(artifact_dir, capture_name)
    app_command = build_headless_goggles_command(
        goggles_binary=goggles_binary,
        target_binary=target_binary,
        goggles_args=goggles_args,
        target_args=target_args,
    )
    command = build_rdc_capture_command(
        capture_path=capture_path,
        app_command=app_command,
        rdc_binary=rdc_binary,
        capture_args=capture_args,
    )
    command_text = _command_text(command)

    completed = run_subprocess(
        command,
        cwd=cwd,
        env=env,
        timeout_seconds=timeout_seconds,
        artifact_dir=artifact_dir,
        check=False,
    )

    metadata = CaptureMetadata(
        capture_path=capture_path,
        invoked_command=command_text,
        stdout=completed.stdout,
        stderr=completed.stderr,
        exit_code=completed.returncode,
    )

    if completed.returncode != 0:
        raise RuntimeError(
            format_capture_error(
                message="rdc capture command failed.",
                capture_path=capture_path,
                command=command_text,
                exit_code=completed.returncode,
                stdout=completed.stdout,
                stderr=completed.stderr,
            )
        )

    _ = assert_capture_exists(
        capture_path,
        command_text,
        stdout=completed.stdout,
        stderr=completed.stderr,
        exit_code=completed.returncode,
    )
    return metadata
