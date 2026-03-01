# Change: Test Framework Phase 3 - RenderDoc GPU State Validation

## Problem

Phase 1 and Phase 2 established deterministic headless visual testing for geometry and basic shader output, but they only validate final pixel results. Goggles still lacks an automated way to assert GPU pipeline state and validation-layer cleanliness during render execution, so regressions in pipeline configuration can pass image-based tests undetected.

## Why

The approved plan in `.omc/plans/comprehensive-test-framework.md` identifies Phase 3 as the next unblocked step after Phase 2. This phase introduces RenderDoc and `rdc-cli` as test instrumentation so the project can verify GPU behavior directly (pipeline topology, validation-layer health, and deterministic frame diffs), not only output images.

## Scope

- Add RenderDoc as a Pixi-managed dependency path, including a local package recipe in `packages/renderdoc/recipe.yaml`.
- Add a GPU test suite under `tests/gpu/` using `pytest` and `rdc-cli` for capture/assert workflows.
- Add shared capture helpers for launching headless goggles runs and collecting `.rdc` captures.
- Add a `pixi` task wrapper (`test-gpu`) for repeatable local GPU validation runs.
- Add two-frame diff tests for shader toggles, parameter changes, and static-scene determinism thresholds.

## Non-goals

- CI workflow and hardware-tier orchestration updates (`.github/workflows/*`) from Phase 6.
- SwiftShader integration and CI determinism policy changes from Phase 6.
- New compositor surface-composition automation from Phase 4.
- Expanding shader matrix coverage beyond Phase 3's targeted zfast-crt state assertions.

## What Changes

### New files

| File | Purpose |
|------|---------|
| `packages/renderdoc/recipe.yaml` | Local pixi-build recipe for RenderDoc artifacts needed by Python/API tooling |
| `tests/gpu/CMakeLists.txt` | GPU test registration/wiring into test build surface |
| `tests/gpu/conftest.py` | Pytest fixtures for capture session setup/teardown and artifact paths |
| `tests/gpu/capture_helper.py` | Shared helper to launch goggles + test client and return capture paths |
| `tests/gpu/test_validation_layers.py` | Validation-layer cleanliness assertions using `rdc assert-clean` |
| `tests/gpu/test_shader_state.py` | Pipeline-state assertions for shader-enabled captures |
| `tests/gpu/test_pixel_accuracy.py` | Pixel-value assertions at fixed coordinates via `rdc assert-pixel` |
| `tests/gpu/test_frame_diff.py` | Two-frame diff coverage for toggles, parameter change, and static stability |
| `scripts/task/test-gpu.sh` | Wrapper script for running GPU pytest suite via pixi task |

### Modified files

| File | Change |
|------|--------|
| `pixi.toml` | Add RenderDoc package/deps and `test-gpu` task wiring |
| `tests/CMakeLists.txt` | Include `tests/gpu/` test subtree registration |

## Impact

- **Affected specs:** `dependency-management`, `visual-regression`
- **Affected code/test modules:** `tests/gpu/`, `scripts/task/`, `pixi.toml`, `packages/renderdoc/`
- **Runtime impact:** No production runtime behavior change; this is test/tooling surface only

## Policy-sensitive impacts

- **Error handling:** GPU test helpers and fixtures must fail fast with actionable errors and include capture paths in failures.
- **Threading:** Phase 3 test code must not introduce direct render/pipeline threading in application code paths.
- **Vulkan API split:** GPU state assertions run externally through RenderDoc tooling; no changes to app-side Vulkan ownership model are introduced.
- **Ownership/lifetime:** Temporary capture artifacts and subprocess handles are managed with deterministic cleanup in fixtures.

## Risks

| Risk | Severity | Likelihood | Mitigation |
|------|----------|-----------|------------|
| RenderDoc build/packaging complexity across distros | MEDIUM | MEDIUM | Keep recipe isolated in `packages/renderdoc`; validate import/version commands as hard acceptance criteria |
| Driver variability affecting diff thresholds | MEDIUM | HIGH | Use explicit thresholds and focus on deterministic scenarios; defer CI standardization to SwiftShader Phase 6 |
| Test failures with poor debuggability | HIGH | LOW | Require all failing assertions to print `.rdc` capture path and command context |
| Longer local test cycle for GPU tier | LOW | MEDIUM | Keep command isolated behind `pixi run test-gpu`; preserve fast existing unit/integration tiers |

## Validation Plan

1. `pixi run python -c "import renderdoc"` succeeds.
2. `pixi run rdc --version` succeeds.
3. `pixi run test-gpu` runs all tests under `tests/gpu/` and reports pass/fail per case.
4. Validation-layer test confirms no HIGH-severity issues in clean baseline capture.
5. Shader-state test confirms expected topology/state for zfast-crt capture.
6. Pixel-accuracy test confirms expected values at fixed sample points.
7. Frame-diff tests enforce thresholds:
   - shader toggle diff > 1%
   - static-scene stability diff < 0.01%
   - parameter-change diff non-zero and localized
8. On any failure, test output includes `.rdc` capture path and failing assertion details.
