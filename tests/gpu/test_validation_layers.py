from __future__ import annotations

import importlib
import importlib.util
import shlex
import subprocess
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import NoReturn, Protocol, cast


class RaisesContextFixture(Protocol):
    value: BaseException

    def __enter__(self) -> "RaisesContextFixture": ...

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: object,
    ) -> bool | None: ...


class PytestFixture(Protocol):
    def skip(self, reason: str) -> NoReturn: ...

    def raises(
        self, expected_exception: type[BaseException]
    ) -> RaisesContextFixture: ...


pytest = cast(
    PytestFixture,
    cast(object, importlib.import_module("pytest")),
)


class RunHeadlessCaptureResult(Protocol):
    capture_path: Path


class RunHeadlessCaptureFixture(Protocol):
    def __call__(
        self,
        *,
        run_subprocess: "SubprocessRunnerFixture",
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
    ) -> RunHeadlessCaptureResult: ...


class FormatCaptureErrorFixture(Protocol):
    def __call__(
        self,
        *,
        message: str,
        capture_path: Path,
        command: str,
        exit_code: int | None = None,
        stdout: str = "",
        stderr: str = "",
    ) -> str: ...


try:
    from .capture_helper import format_capture_error, run_headless_capture
except ImportError:
    helper_path = Path(__file__).with_name("capture_helper.py")
    helper_spec = importlib.util.spec_from_file_location(
        "tests.gpu.capture_helper",
        helper_path,
    )
    if helper_spec is None or helper_spec.loader is None:
        raise RuntimeError(f"Unable to load capture helper module from {helper_path}")
    helper_module = importlib.util.module_from_spec(helper_spec)
    sys.modules[helper_spec.name] = helper_module
    helper_spec.loader.exec_module(helper_module)
    format_capture_error = cast(
        FormatCaptureErrorFixture,
        cast(object, helper_module.format_capture_error),
    )
    run_headless_capture = cast(
        RunHeadlessCaptureFixture,
        cast(object, helper_module.run_headless_capture),
    )

_ASSERT_CLEAN_MIN_SEVERITY = "HIGH"
_ASSERT_CLEAN_COMMAND_TEXT = "rdc assert-clean --min-severity HIGH"


def _skip_if_rdc_command_unavailable(message: str) -> None:
    lower_message = message.lower()
    if (
        "not a valid command" in lower_message
        or "unknown command" in lower_message
        or "renderdoccmd <command>" in lower_message
    ) and "rdc" in lower_message:
        pytest.skip("rdc assert-clean is unavailable in this environment")


class BinaryPathsFixture(Protocol):
    goggles: Path
    quadrant_client: Path


class SubprocessPolicyFixture(Protocol):
    timeout_seconds: int


class SubprocessRunnerFixture(Protocol):
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


def _run_assert_clean(
    *,
    run_subprocess: SubprocessRunnerFixture,
    artifact_dir: Path,
    capture_path: Path,
    min_severity: str,
) -> None:
    open_command = ["rdc", "open", str(capture_path)]
    open_result = run_subprocess(open_command, artifact_dir=artifact_dir, check=False)
    if open_result.returncode != 0:
        raise RuntimeError(
            format_capture_error(
                message="rdc open command failed before assert-clean.",
                capture_path=capture_path,
                command=shlex.join(open_command),
                exit_code=open_result.returncode,
                stdout=open_result.stdout,
                stderr=open_result.stderr,
            )
        )

    assert_clean_command = ["rdc", "assert-clean", "--min-severity", min_severity]
    try:
        assert_clean_result = run_subprocess(
            assert_clean_command,
            artifact_dir=artifact_dir,
            check=False,
        )
    finally:
        _ = run_subprocess(["rdc", "close"], artifact_dir=artifact_dir, check=False)

    if assert_clean_result.returncode != 0:
        raise RuntimeError(
            format_capture_error(
                message="rdc assert-clean command failed.",
                capture_path=capture_path,
                command=shlex.join(assert_clean_command),
                exit_code=assert_clean_result.returncode,
                stdout=assert_clean_result.stdout,
                stderr=assert_clean_result.stderr,
            )
        )


def _capture_deterministic_frame(
    *,
    gpu_preflight: BinaryPathsFixture,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
) -> Path:
    metadata = run_headless_capture(
        run_subprocess=run_subprocess,
        artifact_dir=test_artifact_dir,
        goggles_binary=gpu_preflight.goggles,
        target_binary=gpu_preflight.quadrant_client,
        timeout_seconds=subprocess_policy.timeout_seconds,
        goggles_args=("--frames", "5"),
    )
    return metadata.capture_path


def test_clean_validation_layers_for_deterministic_capture(
    gpu_preflight: BinaryPathsFixture,
    binary_paths: BinaryPathsFixture,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
) -> None:
    assert gpu_preflight.goggles == binary_paths.goggles
    assert gpu_preflight.quadrant_client == binary_paths.quadrant_client

    capture_path = _capture_deterministic_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
    )

    try:
        _run_assert_clean(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            capture_path=capture_path,
            min_severity=_ASSERT_CLEAN_MIN_SEVERITY,
        )
    except RuntimeError as exc:
        _skip_if_rdc_command_unavailable(str(exc))
        raise

    assert capture_path.suffix == ".rdc"
    assert _ASSERT_CLEAN_COMMAND_TEXT.startswith("rdc assert-clean")


def test_reports_capture_path_for_assert_clean_failure(
    gpu_preflight: BinaryPathsFixture,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
) -> None:
    capture_path = _capture_deterministic_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
    )

    with pytest.raises(RuntimeError) as exc_info:
        _run_assert_clean(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            capture_path=capture_path,
            min_severity="NOT_A_REAL_SEVERITY",
        )

    message = str(exc_info.value)
    assert str(capture_path) in message
    assert ".rdc" in message
    assert "Command:" in message
    assert "rdc assert-clean --min-severity NOT_A_REAL_SEVERITY" in message
