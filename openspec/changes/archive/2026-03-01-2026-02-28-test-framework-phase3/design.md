## Context

Phase 2 verifies rendered output quality but does not assert GPU pipeline state. For Phase 3, RenderDoc instrumentation is added to inspect captured frame internals and enforce contracts that image-only checks cannot express (validation layer cleanliness, shader pipeline state, and deterministic frame-to-frame behavior).

## Goals / Non-Goals

**Goals:**
- Introduce RenderDoc and `rdc-cli` as first-class test tooling in Pixi workflows.
- Provide repeatable GPU tests for state validation and frame-diff assertions.
- Keep GPU tests isolated from existing fast test tiers.
- Ensure failures are diagnosable through persisted capture artifacts.

**Non-goals:**
- CI orchestration and hardware pool design (Phase 6).
- Surface composition and compositor behavior automation (Phase 4).
- Broad shader-effect matrix expansion (Phase 5).

## Key Decisions

- Decision: Use a dedicated `tests/gpu/` pytest suite instead of extending Catch2 visual binaries.
  - Rationale: RenderDoc CLI and Python integration are naturally script/test-runner oriented.

- Decision: Standardize capture orchestration in `capture_helper.py` shared by all GPU tests.
  - Rationale: Avoid duplicated process-launch logic and ensure consistent artifact/error reporting.

- Decision: Treat `.rdc` capture path emission as a mandatory failure contract.
  - Rationale: GPU-state failures are difficult to debug without direct capture references.

- Decision: Keep threshold-sensitive assertions explicit in tests (`>1%`, `<0.01%`, etc.).
  - Rationale: Makes regression intent auditable and stable across updates.

## Architecture Outline

1. **Environment layer**
   - Pixi provides RenderDoc package and CLI dependencies.
   - `pixi run test-gpu` is the entrypoint.

2. **Capture layer**
   - `capture_helper.py` launches headless goggles + deterministic test client.
   - Captures are stored under a per-test temporary artifact root.

3. **Assertion layer**
   - `test_validation_layers.py`: validation cleanliness checks.
   - `test_shader_state.py`: pipeline state checks for shader-enabled run.
   - `test_pixel_accuracy.py`: coordinate-level pixel assertions.
   - `test_frame_diff.py`: temporal and parametric difference checks.

4. **Diagnostics layer**
   - Every assertion failure prints capture path and failing command context.

## Risks / Trade-offs

- RenderDoc dependency increases setup complexity.
  - Mitigation: keep dependency contract minimal and validate with explicit import/version checks.

- Frame-diff thresholds may need tuning across drivers.
  - Mitigation: encode thresholds in tests and keep CI standardization for Phase 6.

- GPU tests are slower than unit/integration tests.
  - Mitigation: isolate GPU tests behind dedicated task and labels.

## Verification Strategy

- Verify toolchain availability (`import renderdoc`, `rdc --version`).
- Verify complete suite via `pixi run test-gpu`.
- Verify targeted test files for focused debugging.
- Validate failure output always includes `.rdc` capture path.
