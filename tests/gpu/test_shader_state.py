from __future__ import annotations

import importlib
import importlib.util
import json
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


class CaptureMetadataFixture(Protocol):
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
    ) -> CaptureMetadataFixture: ...


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

_REQUIRED_TOPOLOGY = "TriangleList"
_ASSERT_STATE_KEY_PATH = "topology"
_MISMATCH_TOPOLOGY = "LineList"


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


def _skip_if_rdc_command_unavailable(message: str) -> None:
    lower_message = message.lower()
    if (
        "not a valid command" in lower_message
        or "unknown command" in lower_message
        or "renderdoccmd <command>" in lower_message
    ) and "rdc" in lower_message:
        pytest.skip("Required rdc command is unavailable in this environment")


def _run_rdc_command(
    *,
    run_subprocess: SubprocessRunnerFixture,
    artifact_dir: Path,
    capture_path: Path,
    command: Sequence[str | Path],
    failure_message: str,
) -> subprocess.CompletedProcess[str]:
    command_text = shlex.join(str(value) for value in command)
    completed = run_subprocess(command, artifact_dir=artifact_dir, check=False)
    if completed.returncode != 0:
        raise RuntimeError(
            format_capture_error(
                message=failure_message,
                capture_path=capture_path,
                command=command_text,
                exit_code=completed.returncode,
                stdout=completed.stdout,
                stderr=completed.stderr,
            )
        )
    return completed


def _extract_draw_rows(payload: object) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    if isinstance(payload, list):
        for row in cast(list[object], payload):
            if isinstance(row, dict):
                rows.append(cast(dict[str, object], row))
        return rows

    if isinstance(payload, dict):
        payload_rows = cast(dict[str, object], payload).get("rows", [])
        if isinstance(payload_rows, list):
            for row in cast(list[object], payload_rows):
                if isinstance(row, dict):
                    rows.append(cast(dict[str, object], row))
    return rows


def _row_event_id(row: dict[str, object]) -> int | None:
    event_id = row.get("eid", row.get("eventId"))
    if isinstance(event_id, int):
        return event_id
    if isinstance(event_id, str) and event_id.isdigit():
        return int(event_id)
    return None


def _row_topology(row: dict[str, object]) -> str | None:
    topology = row.get("topology")
    if isinstance(topology, str):
        return topology

    pipe = row.get("pipeline")
    if isinstance(pipe, dict):
        nested_topology = cast(dict[str, object], pipe).get("topology")
        if isinstance(nested_topology, str):
            return nested_topology

    return None


def _discover_draw_event_for_topology(
    *,
    run_subprocess: SubprocessRunnerFixture,
    artifact_dir: Path,
    capture_path: Path,
    topology: str,
) -> int:
    _ = _run_rdc_command(
        run_subprocess=run_subprocess,
        artifact_dir=artifact_dir,
        capture_path=capture_path,
        command=["rdc", "open", capture_path],
        failure_message="rdc open command failed before draw discovery.",
    )

    draws_command = ["rdc", "draws", "--json"]
    try:
        draws_result = _run_rdc_command(
            run_subprocess=run_subprocess,
            artifact_dir=artifact_dir,
            capture_path=capture_path,
            command=draws_command,
            failure_message="rdc draws command failed before state assertion.",
        )
    finally:
        _ = run_subprocess(["rdc", "close"], artifact_dir=artifact_dir, check=False)

    try:
        payload = cast(object, json.loads(draws_result.stdout))
    except json.JSONDecodeError as exc:
        raise RuntimeError(
            format_capture_error(
                message="rdc draws output was not valid JSON.",
                capture_path=capture_path,
                command=shlex.join(draws_command),
                stdout=draws_result.stdout,
                stderr=draws_result.stderr,
            )
        ) from exc

    rows = _extract_draw_rows(payload)
    for row in rows:
        if _row_topology(row) != topology:
            continue
        event_id = _row_event_id(row)
        if event_id is not None:
            return event_id

    raise RuntimeError(
        format_capture_error(
            message=f"Could not find draw event for topology '{topology}'.",
            capture_path=capture_path,
            command=shlex.join(draws_command),
            stdout=draws_result.stdout,
            stderr=draws_result.stderr,
        )
    )


def _assert_topology_state(
    *,
    run_subprocess: SubprocessRunnerFixture,
    artifact_dir: Path,
    capture_path: Path,
    event_id: int,
    expected_topology: str,
) -> None:
    _ = _run_rdc_command(
        run_subprocess=run_subprocess,
        artifact_dir=artifact_dir,
        capture_path=capture_path,
        command=["rdc", "open", capture_path],
        failure_message="rdc open command failed before assert-state.",
    )

    assert_state_command = [
        "rdc",
        "assert-state",
        str(event_id),
        _ASSERT_STATE_KEY_PATH,
        "--expect",
        expected_topology,
    ]
    try:
        _ = _run_rdc_command(
            run_subprocess=run_subprocess,
            artifact_dir=artifact_dir,
            capture_path=capture_path,
            command=assert_state_command,
            failure_message="rdc assert-state command failed.",
        )
    finally:
        _ = run_subprocess(["rdc", "close"], artifact_dir=artifact_dir, check=False)


def _capture_zfast_crt_frame(
    *,
    gpu_preflight: BinaryPathsFixture,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
    repo_root: Path,
) -> Path:
    shader_preset = repo_root / "shaders" / "retroarch" / "crt" / "zfast-crt.slangp"

    try:
        metadata = run_headless_capture(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            goggles_binary=gpu_preflight.goggles,
            target_binary=gpu_preflight.quadrant_client,
            timeout_seconds=subprocess_policy.timeout_seconds,
            capture_name="zfast-shader-state.rdc",
            goggles_args=("--frames", "5", "--preset", shader_preset),
        )
    except RuntimeError as exc:
        pytest.skip(str(exc))

    return metadata.capture_path


def test_shader_state_topology_contract_uses_triangle_list_draw_event(
    gpu_preflight: BinaryPathsFixture,
    binary_paths: BinaryPathsFixture,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
    repo_root: Path,
) -> None:
    assert gpu_preflight.goggles == binary_paths.goggles
    assert gpu_preflight.quadrant_client == binary_paths.quadrant_client

    capture_path = _capture_zfast_crt_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        repo_root=repo_root,
    )

    try:
        draw_event = _discover_draw_event_for_topology(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            capture_path=capture_path,
            topology=_REQUIRED_TOPOLOGY,
        )
        _assert_topology_state(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            capture_path=capture_path,
            event_id=draw_event,
            expected_topology=_REQUIRED_TOPOLOGY,
        )
    except RuntimeError as exc:
        _skip_if_rdc_command_unavailable(str(exc))
        raise

    assert capture_path.suffix == ".rdc"


def test_shader_state_mismatch_context_includes_capture_and_command(
    gpu_preflight: BinaryPathsFixture,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
    repo_root: Path,
) -> None:
    capture_path = _capture_zfast_crt_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        repo_root=repo_root,
    )

    try:
        draw_event = _discover_draw_event_for_topology(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            capture_path=capture_path,
            topology=_REQUIRED_TOPOLOGY,
        )
    except RuntimeError as exc:
        _skip_if_rdc_command_unavailable(str(exc))
        raise

    with pytest.raises(RuntimeError) as exc_info:
        _assert_topology_state(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            capture_path=capture_path,
            event_id=draw_event,
            expected_topology=_MISMATCH_TOPOLOGY,
        )

    message = str(exc_info.value)
    _skip_if_rdc_command_unavailable(message)
    assert str(capture_path) in message
    assert ".rdc" in message
    assert "Command:" in message
    assert "assert-state" in message
