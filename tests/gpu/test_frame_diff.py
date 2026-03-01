from __future__ import annotations

import importlib
import importlib.util
import json
import shlex
import subprocess
import sys
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
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
        self,
        expected_exception: type[BaseException],
        match: str | None = None,
    ) -> RaisesContextFixture: ...


pytest = cast(PytestFixture, cast(object, importlib.import_module("pytest")))


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


SHADER_TOGGLE_MIN_DIFF_PERCENT = 1.0
PARAMETER_CHANGE_LOCALIZED_MAX_DIFF_PERCENT = 25.0
STATIC_SCENE_MAX_DIFF_PERCENT = 0.01


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


@dataclass(frozen=True)
class DiffMetrics:
    baseline_capture: Path
    candidate_capture: Path
    command: str
    diff_ratio_percent: float
    diff_pixels: int
    total_pixels: int
    diff_image: Path


def _coerce_float(payload: dict[str, object], keys: Sequence[str]) -> float | None:
    for key in keys:
        value = payload.get(key)
        if isinstance(value, (int, float)):
            return float(value)
        if isinstance(value, str):
            try:
                return float(value)
            except ValueError:
                continue
    return None


def _coerce_int(payload: dict[str, object], keys: Sequence[str]) -> int | None:
    float_value = _coerce_float(payload, keys)
    if float_value is None:
        return None
    return int(float_value)


def _diff_ratio_percent(
    payload: dict[str, object], diff_pixels: int, total_pixels: int
) -> float:
    explicit_percent = _coerce_float(payload, ("diff_percent", "diffPercent"))
    if explicit_percent is not None:
        return explicit_percent

    ratio_value = _coerce_float(payload, ("diff_ratio", "diffRatio", "ratio"))
    if ratio_value is not None:
        if 0.0 <= ratio_value <= 1.0:
            return ratio_value * 100.0
        return ratio_value

    if total_pixels > 0:
        return (float(diff_pixels) / float(total_pixels)) * 100.0
    return 0.0


def _skip_if_rdc_diff_unavailable(message: str) -> None:
    lower_message = message.lower()
    if (
        "diff is not a valid command" in lower_message
        or "unknown command" in lower_message
        or "renderdoccmd <command>" in lower_message
    ):
        pytest.skip("Required rdc diff command is unavailable in this environment")


def _ensure_rdc_diff_available(
    *,
    run_subprocess: SubprocessRunnerFixture,
    artifact_dir: Path,
) -> None:
    command = ["rdc", "diff", "--help"]
    command_text = shlex.join(command)
    completed = run_subprocess(command, artifact_dir=artifact_dir, check=False)
    if completed.returncode == 0:
        return

    diagnostic = format_capture_error(
        message="rdc diff command is unavailable.",
        capture_path=artifact_dir / "rdc-diff-capability-check.rdc",
        command=command_text,
        exit_code=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
    )
    _skip_if_rdc_diff_unavailable(diagnostic)
    raise RuntimeError(diagnostic)


def _build_rdc_diff_command(
    *,
    baseline_capture: Path,
    candidate_capture: Path,
    diff_image: Path,
) -> list[str]:
    return [
        "rdc",
        "diff",
        str(baseline_capture),
        str(candidate_capture),
        "--framebuffer",
        "--json",
        "--threshold",
        "0",
        "--diff-output",
        str(diff_image),
    ]


def _run_rdc_diff(
    *,
    run_subprocess: SubprocessRunnerFixture,
    artifact_dir: Path,
    baseline_capture: Path,
    candidate_capture: Path,
) -> DiffMetrics:
    diff_image = (
        artifact_dir / f"{baseline_capture.stem}-vs-{candidate_capture.stem}.png"
    )
    command = _build_rdc_diff_command(
        baseline_capture=baseline_capture,
        candidate_capture=candidate_capture,
        diff_image=diff_image,
    )
    command_text = shlex.join(command)
    completed = run_subprocess(command, artifact_dir=artifact_dir, check=False)

    if completed.returncode not in (0, 1):
        raise RuntimeError(
            format_capture_error(
                message="rdc diff command failed.",
                capture_path=candidate_capture,
                command=command_text,
                exit_code=completed.returncode,
                stdout=completed.stdout,
                stderr=completed.stderr,
            )
        )

    try:
        payload_data = cast(object, json.loads(completed.stdout))
    except json.JSONDecodeError as exc:
        raise RuntimeError(
            format_capture_error(
                message="rdc diff output was not valid JSON.",
                capture_path=candidate_capture,
                command=command_text,
                exit_code=completed.returncode,
                stdout=completed.stdout,
                stderr=completed.stderr,
            )
        ) from exc

    if not isinstance(payload_data, dict):
        raise RuntimeError(
            format_capture_error(
                message="rdc diff JSON output had an unexpected shape.",
                capture_path=candidate_capture,
                command=command_text,
                exit_code=completed.returncode,
                stdout=completed.stdout,
                stderr=completed.stderr,
            )
        )

    payload = cast(dict[str, object], payload_data)

    diff_pixels = (
        _coerce_int(payload, ("diff_pixels", "diffPixels", "different_pixels")) or 0
    )
    total_pixels = _coerce_int(payload, ("total_pixels", "totalPixels", "pixels")) or 0
    ratio_percent = _diff_ratio_percent(payload, diff_pixels, total_pixels)

    diff_image_from_payload = payload.get("diff_image", payload.get("diffImage"))
    resolved_diff_image = (
        Path(diff_image_from_payload)
        if isinstance(diff_image_from_payload, str)
        else diff_image
    )

    return DiffMetrics(
        baseline_capture=baseline_capture,
        candidate_capture=candidate_capture,
        command=command_text,
        diff_ratio_percent=ratio_percent,
        diff_pixels=diff_pixels,
        total_pixels=total_pixels,
        diff_image=resolved_diff_image,
    )


def _threshold_artifact_path(*, artifact_dir: Path, contract_name: str) -> Path:
    return artifact_dir / f"{contract_name}-rdc-diff-threshold-violation.txt"


def _write_threshold_artifact(
    *,
    artifact_path: Path,
    contract_name: str,
    metrics: DiffMetrics,
    violations: list[str],
) -> None:
    artifact_lines = [
        f"Contract: {contract_name}",
        f"Baseline capture: {metrics.baseline_capture}",
        f"Candidate capture: {metrics.candidate_capture}",
        f"Diff ratio (%): {metrics.diff_ratio_percent:.6f}",
        f"Diff pixels: {metrics.diff_pixels}",
        f"Total pixels: {metrics.total_pixels}",
        f"Diff image: {metrics.diff_image}",
        f"Command: {metrics.command}",
        "Violations:",
        *[f"- {violation}" for violation in violations],
    ]
    _ = artifact_path.write_text("\n".join(artifact_lines) + "\n", encoding="utf-8")


def _assert_threshold_contract(
    *,
    metrics: DiffMetrics,
    contract_name: str,
    artifact_dir: Path,
    min_diff_percent_exclusive: float | None = None,
    max_diff_percent_exclusive: float | None = None,
    require_non_zero_diff_pixels: bool = False,
) -> None:
    violations: list[str] = []

    if (
        min_diff_percent_exclusive is not None
        and metrics.diff_ratio_percent <= min_diff_percent_exclusive
    ):
        violations.append(
            f"Expected diff ratio > {min_diff_percent_exclusive:.6f}%, got {metrics.diff_ratio_percent:.6f}%"
        )

    if (
        max_diff_percent_exclusive is not None
        and metrics.diff_ratio_percent >= max_diff_percent_exclusive
    ):
        violations.append(
            f"Expected diff ratio < {max_diff_percent_exclusive:.6f}%, got {metrics.diff_ratio_percent:.6f}%"
        )

    if require_non_zero_diff_pixels and metrics.diff_pixels <= 0:
        violations.append(f"Expected non-zero diff pixels, got {metrics.diff_pixels}")

    if not violations:
        return

    artifact_path = _threshold_artifact_path(
        artifact_dir=artifact_dir,
        contract_name=contract_name,
    )
    _write_threshold_artifact(
        artifact_path=artifact_path,
        contract_name=contract_name,
        metrics=metrics,
        violations=violations,
    )
    raise AssertionError(
        "\n".join(
            [
                f"{contract_name} threshold contract failed.",
                f"Diff artifact: {artifact_path}",
                f"Command: {metrics.command}",
                *violations,
            ]
        )
    )


def _write_parameter_override_preset(
    *,
    artifact_dir: Path,
    shader_path: Path,
    parameter_value: float,
    preset_name: str,
) -> Path:
    preset_path = artifact_dir / preset_name
    _ = preset_path.write_text(
        "\n".join(
            [
                "shaders = 1",
                f"shader0 = {shader_path}",
                "filter_linear0 = true",
                "scale_type0 = viewport",
                f"MASK_DARK = {parameter_value:.4f}",
                "",
            ]
        ),
        encoding="utf-8",
    )
    return preset_path


def _capture_frame(
    *,
    gpu_preflight: BinaryPathsFixture,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
    capture_name: str,
    preset: Path | None = None,
) -> Path:
    goggles_args: list[str | Path] = ["--frames", "5"]
    if preset is not None:
        goggles_args.extend(["--preset", preset])

    metadata = run_headless_capture(
        run_subprocess=run_subprocess,
        artifact_dir=test_artifact_dir,
        goggles_binary=gpu_preflight.goggles,
        target_binary=gpu_preflight.quadrant_client,
        timeout_seconds=subprocess_policy.timeout_seconds,
        capture_name=capture_name,
        goggles_args=tuple(goggles_args),
    )

    return metadata.capture_path


def test_shader_toggle_diff_exceeds_one_percent(
    gpu_preflight: BinaryPathsFixture,
    binary_paths: BinaryPathsFixture,
    repo_root: Path,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
) -> None:
    assert gpu_preflight.goggles == binary_paths.goggles
    assert gpu_preflight.quadrant_client == binary_paths.quadrant_client

    _ensure_rdc_diff_available(
        run_subprocess=run_subprocess,
        artifact_dir=test_artifact_dir,
    )

    shader_preset = repo_root / "shaders" / "retroarch" / "crt" / "zfast-crt.slangp"
    baseline_capture = _capture_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        capture_name="shader-off.rdc",
    )
    candidate_capture = _capture_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        capture_name="shader-on.rdc",
        preset=shader_preset,
    )

    try:
        metrics = _run_rdc_diff(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            baseline_capture=baseline_capture,
            candidate_capture=candidate_capture,
        )
    except RuntimeError as exc:
        _skip_if_rdc_diff_unavailable(str(exc))
        raise

    _assert_threshold_contract(
        metrics=metrics,
        contract_name="shader_toggle",
        artifact_dir=test_artifact_dir,
        min_diff_percent_exclusive=SHADER_TOGGLE_MIN_DIFF_PERCENT,
    )


def test_parameter_change_diff_is_non_zero_and_localized(
    gpu_preflight: BinaryPathsFixture,
    binary_paths: BinaryPathsFixture,
    repo_root: Path,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
) -> None:
    assert gpu_preflight.goggles == binary_paths.goggles
    assert gpu_preflight.quadrant_client == binary_paths.quadrant_client

    _ensure_rdc_diff_available(
        run_subprocess=run_subprocess,
        artifact_dir=test_artifact_dir,
    )

    shader_path = (
        repo_root
        / "shaders"
        / "retroarch"
        / "crt"
        / "shaders"
        / "zfast_crt"
        / "zfast_crt_finemask.slang"
    )
    baseline_preset = _write_parameter_override_preset(
        artifact_dir=test_artifact_dir,
        shader_path=shader_path,
        parameter_value=0.25,
        preset_name="parameter-baseline.slangp",
    )
    candidate_preset = _write_parameter_override_preset(
        artifact_dir=test_artifact_dir,
        shader_path=shader_path,
        parameter_value=0.90,
        preset_name="parameter-candidate.slangp",
    )

    baseline_capture = _capture_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        capture_name="parameter-a.rdc",
        preset=baseline_preset,
    )
    candidate_capture = _capture_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        capture_name="parameter-b.rdc",
        preset=candidate_preset,
    )

    try:
        metrics = _run_rdc_diff(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            baseline_capture=baseline_capture,
            candidate_capture=candidate_capture,
        )
    except RuntimeError as exc:
        _skip_if_rdc_diff_unavailable(str(exc))
        raise

    _assert_threshold_contract(
        metrics=metrics,
        contract_name="parameter_change",
        artifact_dir=test_artifact_dir,
        min_diff_percent_exclusive=0.0,
        max_diff_percent_exclusive=PARAMETER_CHANGE_LOCALIZED_MAX_DIFF_PERCENT,
        require_non_zero_diff_pixels=True,
    )


def test_static_scene_stability_diff_below_point_zero_one_percent(
    gpu_preflight: BinaryPathsFixture,
    binary_paths: BinaryPathsFixture,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
) -> None:
    assert gpu_preflight.goggles == binary_paths.goggles
    assert gpu_preflight.quadrant_client == binary_paths.quadrant_client

    _ensure_rdc_diff_available(
        run_subprocess=run_subprocess,
        artifact_dir=test_artifact_dir,
    )

    baseline_capture = _capture_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        capture_name="static-frame-a.rdc",
    )
    candidate_capture = _capture_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        capture_name="static-frame-b.rdc",
    )

    try:
        metrics = _run_rdc_diff(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            baseline_capture=baseline_capture,
            candidate_capture=candidate_capture,
        )
    except RuntimeError as exc:
        _skip_if_rdc_diff_unavailable(str(exc))
        raise

    _assert_threshold_contract(
        metrics=metrics,
        contract_name="static_stability",
        artifact_dir=test_artifact_dir,
        max_diff_percent_exclusive=STATIC_SCENE_MAX_DIFF_PERCENT,
    )


def test_threshold_violation_writes_diff_artifact_and_command_context(
    gpu_preflight: BinaryPathsFixture,
    binary_paths: BinaryPathsFixture,
    repo_root: Path,
    test_artifact_dir: Path,
    run_subprocess: SubprocessRunnerFixture,
    subprocess_policy: SubprocessPolicyFixture,
) -> None:
    assert gpu_preflight.goggles == binary_paths.goggles
    assert gpu_preflight.quadrant_client == binary_paths.quadrant_client

    _ensure_rdc_diff_available(
        run_subprocess=run_subprocess,
        artifact_dir=test_artifact_dir,
    )

    shader_preset = repo_root / "shaders" / "retroarch" / "crt" / "zfast-crt.slangp"
    baseline_capture = _capture_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        capture_name="violation-shader-off.rdc",
    )
    candidate_capture = _capture_frame(
        gpu_preflight=gpu_preflight,
        test_artifact_dir=test_artifact_dir,
        run_subprocess=run_subprocess,
        subprocess_policy=subprocess_policy,
        capture_name="violation-shader-on.rdc",
        preset=shader_preset,
    )

    try:
        metrics = _run_rdc_diff(
            run_subprocess=run_subprocess,
            artifact_dir=test_artifact_dir,
            baseline_capture=baseline_capture,
            candidate_capture=candidate_capture,
        )
    except RuntimeError as exc:
        _skip_if_rdc_diff_unavailable(str(exc))
        raise

    contract_name = "threshold_violation_probe"
    artifact_path = _threshold_artifact_path(
        artifact_dir=test_artifact_dir,
        contract_name=contract_name,
    )
    impossible_min_diff = metrics.diff_ratio_percent + 0.000001

    with pytest.raises(AssertionError, match="Diff artifact:") as exc_info:
        _assert_threshold_contract(
            metrics=metrics,
            contract_name=contract_name,
            artifact_dir=test_artifact_dir,
            min_diff_percent_exclusive=impossible_min_diff,
        )

    assert artifact_path.is_file()
    artifact_content = artifact_path.read_text(encoding="utf-8")
    assert "Command: rdc diff" in artifact_content
    assert "Diff image:" in artifact_content
    assert "Diff artifact:" in str(exc_info.value)
    assert "Command: rdc diff" in str(exc_info.value)
