# Change: Test Framework Phase 2 — Core Visual Regression Tests

## Problem

Goggles has no automated assertions on render output. Every regression in aspect ratio math, scale mode geometry, shader bypass, or filter-chain application is invisible until a developer manually inspects a screen. The headless mode and test client infrastructure from Phase 1 provide the execution primitive to capture render output deterministically — but no test yet consumes that output to assert correctness. The result is a gap between "the pipeline runs" (smoke test) and "the pipeline renders what we expect" (visual regression).

## Why

Phase 1 delivered the foundational infrastructure: four deterministic test client apps, the `image_compare` library, and a headless pipeline smoke test. That work established that the full render pipeline can be exercised end-to-end without a display. However, no test currently validates *what* the pipeline renders.

Phase 2 closes that gap by introducing the first batch of automated visual tests. Aspect ratio modes are the most load-bearing render-pipeline behaviour: eight distinct scale modes apply different geometric transformations that are mathematically verifiable without golden images. Shader bypass and zfast-crt application are the entry points for visual validation of the filter chain. Golden image management defines the update-and-review workflow that all future visual tests will follow.

This phase completes the "render-what-we-expect" assertion tier and lays the golden image workflow that Phases 3–6 will build on.

## Scope

- **Aspect ratio tests**: `tests/visual/test_aspect_ratio.cpp` — 8 parametrised CTest cases covering `fit`, `fill`, `stretch`, `integer` (1×, 2×, auto), and `dynamic` scale modes using `quadrant_client` as source; assertions are mathematical pixel-region checks, not golden-image comparisons
- **Shader tests**: `tests/visual/test_shader_basic.cpp` — 3 tests: bypass, zfast-crt applied, and filter-chain toggle; golden image comparison with explicit tolerance contracts
- **Per-test TOML configs**: `tests/visual/configs/` — one config file per scenario driving `--config` CLI flag
- **Golden images**: `tests/golden/` directory (created by this change) with `.gitattributes` (Git LFS), a README, and two initial reference images (`shader_bypass_quadrant.png`, `shader_zfast_quadrant.png`)
- **Golden update script**: `scripts/task/update-golden.sh` — created by this change; regenerates golden images from current build via headless mode
- **CMake wiring**: extend `tests/visual/CMakeLists.txt` to build and register all new test executables with `visual` CTest label
- **Pixi task**: `update-golden` in `pixi.toml`

## Non-goals

- Phase 3 (RenderDoc/GPU state validation) — independent change
- Phase 4 (compositor/surface composition tests) — depends on Phase 2 patterns, separate change
- CI workflow updates (`.github/workflows/`) — Phase 6
- SwiftShader integration — Phase 6 (determinism for CI); local runs use the installed Vulkan driver
- Shader effect tests beyond zfast-crt (CRT Royale, HSM Afterglow) — Phase 5
- Runtime UI toggle via ImGui interaction in headless mode — not supported; toggle test uses config-based dual-run (see toggle test clarification below)

## What Changes

### New files (all created by this change — none pre-exist in the repo)

| File | Purpose |
|------|---------|
| `tests/visual/test_aspect_ratio.cpp` | 8 parametrised Catch2 tests; mathematical pixel-region assertions, no golden images |
| `tests/visual/test_shader_basic.cpp` | 3 Catch2 tests: bypass, zfast-crt, toggle (config-based dual-run) |
| `tests/visual/configs/aspect_fit_letterbox.toml` | Config: `fit` mode, 640×480 source, 1920×1080 viewport |
| `tests/visual/configs/aspect_fit_pillarbox.toml` | Config: `fit` mode, 1920×1080 source, 800×600 viewport |
| `tests/visual/configs/aspect_fill.toml` | Config: `fill` mode, 640×480 → 1920×1080 |
| `tests/visual/configs/aspect_stretch.toml` | Config: `stretch` mode, 640×480 → 1920×1080 |
| `tests/visual/configs/aspect_integer_1x.toml` | Config: `integer` mode, scale=1, 320×240 → 1920×1080 |
| `tests/visual/configs/aspect_integer_2x.toml` | Config: `integer` mode, scale=2, 320×240 → 1920×1080 |
| `tests/visual/configs/aspect_integer_auto.toml` | Config: `integer` mode, scale=0 (auto), 320×240 → 1920×1080 |
| `tests/visual/configs/aspect_dynamic.toml` | Config: `dynamic` mode, 640×480 → 1920×1080 |
| `tests/visual/configs/shader_bypass.toml` | Config: no shader preset |
| `tests/visual/configs/shader_zfast.toml` | Config: zfast-crt preset |
| `tests/golden/.gitattributes` | `*.png filter=lfs diff=lfs merge=lfs -text` |
| `tests/golden/README.md` | Golden image workflow documentation |
| `tests/golden/shader_bypass_quadrant.png` | Generated reference — unshaded quadrant output (created by `update-golden`) |
| `tests/golden/shader_zfast_quadrant.png` | Generated reference — zfast-crt applied to quadrant (created by `update-golden`) |
| `scripts/task/update-golden.sh` | Regenerates all golden images via headless mode |

### Modified files

| File | Change |
|------|--------|
| `tests/visual/CMakeLists.txt` | Add `test_aspect_ratio` and `test_shader_basic` executables; register with CTest `visual` label |
| `pixi.toml` | Add `update-golden` task |

## Toggle Test Clarification

The filter-chain toggle test does **not** rely on runtime ImGui toggle (not accessible in headless mode). It is implemented as a **config-based dual-run**:

1. Spawn `goggles --headless --frames 5 --output /tmp/toggle_on.png --config shader_zfast.toml -- quadrant_client` → capture output
2. Spawn `goggles --headless --frames 5 --output /tmp/toggle_off.png --config shader_bypass.toml -- quadrant_client` → capture output
3. Assert `toggle_on.png` matches `shader_zfast_quadrant.png` golden within zfast tolerance
4. Assert `toggle_off.png` matches `shader_bypass_quadrant.png` golden within bypass tolerance

Each invocation is a fully independent process. There is no shared state between the two runs. This is deliberately deterministic: it validates that the two config paths produce correctly distinct outputs, not the runtime toggle mechanism (which is Phase 4 territory).

## Tolerance Contract

All comparisons use `compare_images(actual, reference, tolerance_per_channel)` from `tests/visual/image_compare.hpp`. Pass/fail is gated on `CompareResult.passed`.

| Test | `tolerance_per_channel` | `max_failing_percentage` | Rationale |
|------|------------------------|--------------------------|-----------|
| Aspect ratio — border/black-bar sampling | `0.0` (exact) | 0% | CPU-rendered `wl_shm` source; black regions must be exactly (0,0,0) |
| Aspect ratio — content quadrant sampling | `2.0/255.0` (~0.008) | 0.5% | Allows ±1 LSB from bilinear filtering at content boundaries |
| Shader bypass golden comparison | `2.0/255.0` (~0.008) | 0.1% | No shader transform; output should be near-identical to reference |
| zfast-crt golden comparison | `0.05` (≈13/255) | 5.0% | CRT scanline effects vary slightly across GPU drivers |

`max_failing_percentage` is enforced by asserting `CompareResult.failing_percentage <= threshold` in addition to `CompareResult.passed`. Both fields MUST satisfy their thresholds for a test to pass.

## Spec-Delta Intent

### `openspec/specs/visual-regression/spec.md` — additions

**New Requirement block: Aspect ratio visual regression**

Scenarios to add (one per scale mode):
- GIVEN `quadrant_client` rendering 640×480 with `fit` mode into 1920×1080 / WHEN headless capture runs / THEN top/bottom border pixels SHALL be exactly (0,0,0) AND content pixels at computed positions SHALL match expected quadrant colors within `2.0/255.0` per channel
- (Parallel scenarios for `fill`, `stretch`, `integer` 1×/2×/auto, `dynamic`)

**New Requirement block: Shader visual regression**

Scenarios to add:
- GIVEN bypass config / WHEN headless capture / THEN output SHALL match `shader_bypass_quadrant.png` with `tolerance_per_channel ≤ 2.0/255.0` AND `failing_percentage ≤ 0.1%`
- GIVEN zfast-crt config / WHEN headless capture / THEN output SHALL match `shader_zfast_quadrant.png` with `tolerance_per_channel ≤ 0.05` AND `failing_percentage ≤ 5.0%`
- GIVEN bypass then zfast-crt configs run independently / WHEN outputs compared / THEN each SHALL match its respective golden within its tolerance

**New Requirement block: Golden image management**

Scenarios to add:
- GIVEN `pixi run update-golden` executes / WHEN script completes / THEN all PNG files under `tests/golden/` SHALL be overwritten with fresh headless captures AND exit code SHALL be 0
- GIVEN `tests/golden/*.png` tracked via Git LFS / WHEN repo is cloned / THEN `git lfs pull` SHALL produce valid PNG files

### `openspec/specs/build-system/spec.md` — addition

**New Requirement block: Golden image update task**

Scenario to add:
- GIVEN a clean build at any preset / WHEN `pixi run update-golden` is invoked / THEN `scripts/task/update-golden.sh` SHALL execute, regenerating all files under `tests/golden/` and exiting 0

## CI Survivability

CI workflow changes are deferred to Phase 6, but visual tests registered with the `visual` CTest label MUST NOT silently break existing CI. The containment strategy:

**Label isolation is the gate.** The existing CI invocation pattern runs `ctest --preset test -L unit` and `ctest --preset test -L integration`. Tests with only the `visual` label are excluded from both of those invocations. The new test targets registered in this change MUST carry only the `visual` label — no `unit` or `integration` label is assigned.

**Verification requirement (Validation Plan step 9 below):** Confirm that `ctest --preset test -L unit` and `ctest --preset test -L integration` complete successfully with no new tests included. This is a hard acceptance criterion for this change.

**Risk**: If CI calls bare `ctest --preset test` (no label filter), visual tests will execute without GPU or golden images, failing. This is documented as a known gap; Phase 6 adds the label-gated CI job. If bare `ctest --preset test` is the current CI invocation, an `ENVIRONMENT` property on visual test targets (e.g., `GOGGLES_VISUAL_TESTS=1` required to run) must be added before merge.

## Capabilities

### New Capabilities

- `visual-regression-aspect-ratio`: Automated validation of all 8 scale modes via mathematical pixel-region assertions against `quadrant_client` output — no golden image required for geometry tests
- `visual-regression-shader`: Automated comparison of bypass vs. zfast-crt render output against committed golden images; includes config-based toggle validation
- `golden-image-management`: Reproducible workflow for generating, reviewing, and updating reference images; tracked via Git LFS; regenerated by `pixi run update-golden`

### Modified Capabilities

- `visual-regression` (spec): Extended with aspect ratio requirements, shader requirements, golden image management requirement — see Spec-Delta Intent above
- `build-system` (spec): New `update-golden` pixi task requirement

## Impact

### Impacted modules / files

- `tests/visual/` — new test sources and TOML configs (~2 new C++ files, ~12 TOML files)
- `tests/golden/` — new directory (created by this change) with LFS-tracked PNGs and README
- `scripts/task/update-golden.sh` — new script (created by this change)
- `tests/visual/CMakeLists.txt` — extended with two new test targets
- `pixi.toml` — one new task

### Impacted OpenSpec specs

- `visual-regression` — see Spec-Delta Intent above
- `build-system` — see Spec-Delta Intent above

### Policy-sensitive impacts

- **Error handling**: Catch2 test fixtures that run goggles as a subprocess check exit code; any non-zero exit is a hard `REQUIRE` failure, not a silent skip; temporary output paths are cleaned up via RAII scope guard
- **No threading**: Test code does not spawn threads; each subprocess is launched synchronously and awaited before the next
- **No raw new/delete**: All RAII; subprocess handles and temp-file paths scoped to test fixtures
- **No Vulkan API calls in test code**: Tests interact with goggles exclusively through the CLI interface (`--headless --frames N --output path --config path -- client`)
- **Tolerance documented per test**: Exact constants are specified in the Tolerance Contract table above; tolerance values are `constexpr` named constants in source, not magic numbers

## Risks

| Risk | Severity | Likelihood | Mitigation |
|------|----------|-----------|------------|
| Aspect ratio math differs on fractional viewport dimensions | MEDIUM | LOW | Tests use integer-divisible viewport/source pairs; pixel boundary assertions include ±1 px tolerance (`2.0/255.0`) for filtering artefacts |
| Golden images differ across GPU drivers (shader output varies) | MEDIUM | HIGH | zfast-crt tolerance set to `0.05` / 5%; Phase 6 replaces with SwiftShader-generated goldens for CI determinism |
| Bare `ctest --preset test` in CI picks up visual tests without GPU/goldens | HIGH | LOW | Confirmed by inspection of CI config before merge; if bare invocation exists, add `ENVIRONMENT GOGGLES_VISUAL_TESTS=1` guard (see CI Survivability) |
| `quadrant_client` frame not yet committed when goggles reads frame N | LOW | LOW | Clients commit 30 stable frames; `--frames 5` samples well within the stable window |
| Git LFS not configured in dev environment | LOW | LOW | README documents setup; missing LFS degrades to pointer files, not data corruption; goldens are regeneratable via `update-golden` |

## Validation Plan

1. **Build**: `cmake --preset debug && cmake --build --preset debug` — all targets build including `test_aspect_ratio` and `test_shader_basic` executables
2. **Generate goldens first**: `pixi run update-golden` — exits 0; `tests/golden/shader_bypass_quadrant.png` and `tests/golden/shader_zfast_quadrant.png` exist and are valid PNGs
3. **Aspect ratio tests**: `ctest --preset test -L visual -R aspect` — all 8 tests pass with exit 0
4. **Shader tests**: `ctest --preset test -L visual -R shader` — all 3 tests pass with exit 0
5. **Regression check — unit**: `ctest --preset test -L unit` — all existing unit tests pass; zero new tests included
6. **Regression check — integration**: `ctest --preset test -L integration` — headless smoke test passes; zero new tests included
7. **CI survivability confirmation**: verify CI invocation does not use bare `ctest --preset test`; if it does, add `ENVIRONMENT` guard before merge
8. **Quality gate**: `pixi run build -p quality` — clang-tidy WarningsAsErrors passes on all new C++ sources
9. **Format**: `pixi run format` — no formatting changes required
