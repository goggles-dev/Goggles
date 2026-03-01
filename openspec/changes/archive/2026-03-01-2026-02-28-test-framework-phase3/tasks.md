## 1. RenderDoc Dependency Integration

- [x] 1.1 Add `packages/renderdoc/recipe.yaml` with source pinning and reproducible build metadata for required RenderDoc artifacts.
- [x] 1.2 Update `pixi.toml` to include RenderDoc package wiring and `rdc-cli` availability in the Python environment.
- [x] 1.3 Verify dependency availability:
  - `pixi run python -c "import renderdoc"`
  - `pixi run rdc --version`

## 2. GPU Test Harness Foundation

- [x] 2.1 Create `tests/gpu/CMakeLists.txt` and register the GPU test subtree from `tests/CMakeLists.txt`.
- [x] 2.2 Add `tests/gpu/conftest.py` fixtures for temporary capture directories, process launch helpers, and cleanup behavior.
- [x] 2.3 Add `tests/gpu/capture_helper.py` to encapsulate headless launch/capture flow and standardized error messaging with capture paths.

## 3. GPU Assertion Test Suite

- [x] 3.1 Implement `tests/gpu/test_validation_layers.py` to run clean-capture assertions (`rdc assert-clean --min-severity HIGH`).
- [x] 3.2 Implement `tests/gpu/test_shader_state.py` to verify expected zfast-crt pipeline characteristics from capture metadata/state.
- [x] 3.3 Implement `tests/gpu/test_pixel_accuracy.py` to assert fixed-coordinate pixel expectations using `rdc assert-pixel`.
- [x] 3.4 Implement `tests/gpu/test_frame_diff.py` for:
  - shader toggle measurable diff (>1%)
  - parameter change non-zero localized diff
  - static-scene determinism (<0.01% diff)

## 4. Task Wrapper and Operator UX

- [x] 4.1 Add `scripts/task/test-gpu.sh` to execute `pytest tests/gpu/ -v` with consistent environment handling.
- [x] 4.2 Add `[tasks.test-gpu]` in `pixi.toml` to call the wrapper script.
- [x] 4.3 Ensure all GPU test failures include explicit `.rdc` capture paths in output.

## 5. Spec Updates

- [x] 5.1 Add `dependency-management` spec delta for RenderDoc + `rdc-cli` environment requirements.
- [x] 5.2 Add `visual-regression` spec delta for GPU state assertions and frame-diff behavior contracts.

## 6. Verification

- [x] 6.1 Build verification: `pixi run build -p test`.
- [x] 6.2 Dependency verification:
  - `pixi run python -c "import renderdoc"`
  - `pixi run rdc --version`
- [x] 6.3 GPU suite verification: `pixi run test-gpu`.
- [x] 6.4 Targeted pytest verification:
  - `pixi run pytest tests/gpu/test_validation_layers.py -v`
  - `pixi run pytest tests/gpu/test_shader_state.py -v`
  - `pixi run pytest tests/gpu/test_pixel_accuracy.py -v`
  - `pixi run pytest tests/gpu/test_frame_diff.py -v`
