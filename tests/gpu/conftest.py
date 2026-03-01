from __future__ import annotations

import importlib
import os
import re
import shutil
import subprocess
from collections.abc import Callable, Iterable, Iterator, Mapping
from dataclasses import dataclass
from pathlib import Path

pytest = importlib.import_module("pytest")


_VALID_PRESET_RE = re.compile(r"^[A-Za-z0-9_-]+$")
_REQUIRED_RDC_COMMANDS = ("assert-clean", "assert-state", "assert-pixel", "diff")
_RDC_HELP_TIMEOUT_SECONDS = 10


@dataclass(frozen=True)
class SubprocessPolicy:
    timeout_seconds: int


@dataclass(frozen=True)
class BinaryPaths:
    goggles: Path
    quadrant_client: Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _validate_preset(value: str) -> str:
    if not _VALID_PRESET_RE.fullmatch(value):
        raise pytest.UsageError(
            f"Invalid preset '{value}'. Use only letters, numbers, '-' or '_'."
        )
    return value


def _detect_build_preset(repo_root: Path) -> str:
    preset_from_env = os.environ.get("PRESET") or os.environ.get("GOGGLES_PRESET")
    if preset_from_env:
        return _validate_preset(preset_from_env)

    cwd = Path.cwd().resolve()
    try:
        rel = cwd.relative_to(repo_root)
    except ValueError:
        return "debug"

    if len(rel.parts) >= 2 and rel.parts[0] == "build":
        candidate = rel.parts[1]
        if _VALID_PRESET_RE.fullmatch(candidate):
            return candidate

    return "debug"


def _normalize_candidate(path_value: str, repo_root: Path) -> Path:
    candidate = Path(path_value)
    if not candidate.is_absolute():
        candidate = (repo_root / candidate).resolve()
    return candidate


def _resolve_required_binary(
    *,
    label: str,
    env_vars: tuple[str, ...],
    fallback_candidates: Iterable[Path],
    preset: str,
    repo_root: Path,
) -> Path:
    checked_paths: list[Path] = []

    for env_var in env_vars:
        override = os.environ.get(env_var)
        if not override:
            continue
        override_path = _normalize_candidate(override, repo_root)
        checked_paths.append(override_path)
        if override_path.is_file() and os.access(override_path, os.X_OK):
            return override_path

        checked = "\n  - ".join(str(path) for path in checked_paths)
        raise RuntimeError(
            f"Required binary '{label}' was set via {env_var} but is not executable.\n"
            + f"Checked:\n  - {checked}\nExpected an executable file."
        )

    for candidate in fallback_candidates:
        checked_paths.append(candidate)
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate

    checked = "\n  - ".join(str(path) for path in checked_paths)
    raise RuntimeError(
        f"Required binary '{label}' was not found for preset '{preset}'.\n"
        + f"Checked:\n  - {checked}\n"
        + f"Build the test binaries with: pixi run build -p {preset}\n"
        f"To override discovery, set one of: {', '.join(env_vars)}"
    )


def _sanitize_test_name(nodeid: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9_.-]+", "_", nodeid).strip("_")
    return sanitized or "unnamed_test"


def _timeout_from_env() -> int:
    raw = os.environ.get("GOGGLES_GPU_SUBPROCESS_TIMEOUT", "120")
    try:
        timeout = int(raw)
    except ValueError as exc:
        raise pytest.UsageError(
            "GOGGLES_GPU_SUBPROCESS_TIMEOUT must be an integer number of seconds."
        ) from exc
    if timeout <= 0:
        raise pytest.UsageError("GOGGLES_GPU_SUBPROCESS_TIMEOUT must be > 0.")
    return timeout


def _missing_rdc_contract_commands() -> list[str]:
    missing: list[str] = []
    for command_name in _REQUIRED_RDC_COMMANDS:
        command = ["rdc", command_name, "--help"]
        try:
            result = subprocess.run(
                command,
                check=False,
                capture_output=True,
                text=True,
                timeout=_RDC_HELP_TIMEOUT_SECONDS,
            )
        except (OSError, subprocess.TimeoutExpired):
            missing.append(command_name)
            continue

        if result.returncode != 0:
            missing.append(command_name)

    return missing


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return _repo_root()


@pytest.fixture(scope="session")
def build_preset(repo_root: Path) -> str:
    return _detect_build_preset(repo_root)


@pytest.fixture(scope="session")
def build_root(repo_root: Path, build_preset: str) -> Path:
    return repo_root / "build" / build_preset


@pytest.fixture(scope="session")
def gpu_artifacts_root(build_root: Path) -> Path:
    root = build_root / "tests" / "gpu-artifacts"
    root.mkdir(parents=True, exist_ok=True)
    return root


@pytest.fixture
def test_artifact_dir(request, gpu_artifacts_root: Path) -> Path:
    test_name = _sanitize_test_name(request.node.nodeid)
    artifact_dir = gpu_artifacts_root / test_name

    if artifact_dir.exists():
        shutil.rmtree(artifact_dir)

    artifact_dir.mkdir(parents=True, exist_ok=True)
    return artifact_dir


@pytest.fixture
def scratch_dir(test_artifact_dir: Path) -> Iterator[Path]:
    path = test_artifact_dir / "scratch"
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)
    yield path
    shutil.rmtree(path, ignore_errors=True)


@pytest.fixture(scope="session")
def subprocess_policy() -> SubprocessPolicy:
    return SubprocessPolicy(timeout_seconds=_timeout_from_env())


@pytest.fixture(scope="session")
def binary_paths(build_root: Path, build_preset: str, repo_root: Path) -> BinaryPaths:
    goggles = _resolve_required_binary(
        label="goggles",
        env_vars=(
            "GOGGLES_BINARY",
            "GOGGLES_GPU_GOGGLES_BINARY",
            "GOGGLES_GPU_GOGGLES_BIN",
        ),
        fallback_candidates=(
            build_root / "bin" / "goggles",
            build_root / "src" / "goggles",
            build_root / "goggles",
        ),
        preset=build_preset,
        repo_root=repo_root,
    )
    quadrant_client = _resolve_required_binary(
        label="quadrant_client",
        env_vars=("QUADRANT_CLIENT_BINARY", "GOGGLES_GPU_QUADRANT_CLIENT_BINARY"),
        fallback_candidates=(
            build_root / "tests" / "clients" / "quadrant_client",
            build_root / "tests" / "quadrant_client",
        ),
        preset=build_preset,
        repo_root=repo_root,
    )
    return BinaryPaths(goggles=goggles, quadrant_client=quadrant_client)


@pytest.fixture(scope="session")
def gpu_preflight(binary_paths: BinaryPaths) -> BinaryPaths:
    try:
        _ = importlib.import_module("renderdoc")
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "RenderDoc Python module is unavailable.\n"
            + "Install dependencies with: pixi install --locked\n"
            + 'Verify with: pixi run python -c "import renderdoc"'
        ) from exc

    rdc_path = shutil.which("rdc")
    if rdc_path is None:
        raise RuntimeError(
            "rdc CLI is unavailable in the active Pixi environment.\n"
            + "Install dependencies with: pixi install --locked\n"
            + "Verify with: pixi run rdc --version"
        )

    missing_commands = _missing_rdc_contract_commands()
    if missing_commands:
        missing_text = ", ".join(missing_commands)
        raise RuntimeError(
            "rdc command contract is incomplete in the active Pixi environment.\n"
            + f"Missing subcommands: {missing_text}\n"
            + "Provide an rdc implementation that supports assert-clean/assert-state/assert-pixel/diff.\n"
            + "Verify with: pixi run rdc assert-clean --help"
        )

    return binary_paths


@pytest.fixture
def run_subprocess(
    repo_root: Path,
    subprocess_policy: SubprocessPolicy,
) -> Callable[..., subprocess.CompletedProcess[str]]:
    def _run(
        args: Iterable[str | Path],
        *,
        cwd: Path | None = None,
        env: Mapping[str, str] | None = None,
        timeout_seconds: int | None = None,
        artifact_dir: Path | None = None,
        check: bool = True,
    ) -> subprocess.CompletedProcess[str]:
        command = [str(value) for value in args]
        execution_cwd = str(cwd or repo_root)
        merged_env = os.environ.copy()
        if env:
            merged_env.update(env)

        effective_timeout = timeout_seconds or subprocess_policy.timeout_seconds

        try:
            result = subprocess.run(
                command,
                cwd=execution_cwd,
                env=merged_env,
                check=False,
                capture_output=True,
                text=True,
                timeout=effective_timeout,
            )
        except subprocess.TimeoutExpired as exc:
            artifact_hint = (
                f"\nArtifact dir: {artifact_dir}" if artifact_dir is not None else ""
            )
            raise AssertionError(
                f"Subprocess timed out.\n"
                + f"Command: {' '.join(command)}\n"
                + f"Timeout: {effective_timeout}s\n"
                f"CWD: {execution_cwd}{artifact_hint}"
            ) from exc

        if check and result.returncode != 0:
            artifact_hint = (
                f"\nArtifact dir: {artifact_dir}" if artifact_dir is not None else ""
            )
            raise AssertionError(
                f"Subprocess failed.\n"
                + f"Command: {' '.join(command)}\n"
                + f"Exit code: {result.returncode}\n"
                + f"CWD: {execution_cwd}{artifact_hint}\n"
                + f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            )

        return result

    return _run
