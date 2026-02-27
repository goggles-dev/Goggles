## 1. CMake and CTest Infrastructure

- [x] 1.1 Add `tests/clients/` and `tests/visual/` subdirectories unconditionally to `tests/CMakeLists.txt` (no gating option needed — all deps are already project requirements)
- [x] 1.2 Add CTest label taxonomy: `unit` for Catch2 tests, `integration` for headless smoke, `visual` for future regression tests
- [x] 1.3 Verify `cmake --preset debug && cmake --build --preset debug` builds all test infrastructure (85/85 targets)
- [x] 1.4 Verify `pixi run test -p test` passes all tests including new smoke and image_compare tests (10/10)

## 2. Test Client Apps

- [x] 2.1 Create `tests/clients/CMakeLists.txt` with helper function `add_test_client(name sources...)` that creates an executable with wayland-client and xdg-shell protocol linkage
- [x] 2.2 Implement `tests/clients/wl_helpers.hpp` — thin RAII wrappers for `wl_display*`, `wl_registry*`, `wl_compositor*`, `wl_shm*`, `xdg_wm_base*` using custom deleters (no raw `wl_*_destroy` calls in client code)
- [x] 2.3 Implement `tests/clients/solid_color_client.cpp` — connects via `WAYLAND_DISPLAY`, creates `xdg_toplevel`, allocates a `wl_shm` buffer filled with `TEST_COLOR` (env var, default 255,0,0,255), commits 30 frames, exits 0
- [x] 2.4 Implement `tests/clients/quadrant_client.cpp` — same structure as solid_color but fills four quadrants: red/green/blue/white at W/2 × H/2 boundaries
- [x] 2.5 Implement `tests/clients/gradient_client.cpp` — fills buffer with horizontal gradient R=G=B=x/(W-1)*255 per column
- [x] 2.6 Implement `tests/clients/multi_surface_client.cpp` — creates main surface (blue background) + subsurface (red foreground) at a fixed offset; commits 30 frames each; exits 0
- [x] 2.7 Build and verify all four clients compile under `cmake --build --preset test`

## 3. Image Comparison Library

- [x] 3.1 Create `tests/visual/CMakeLists.txt` with `image_compare` static library target and `goggles_image_compare` CLI executable target; add `stb_image_impl.cpp` TU to the library (using `STB_IMAGE_IMPLEMENTATION`) so it does not conflict with `stb_image_write_impl.cpp`
- [x] 3.2 Define `CompareResult` struct and `compare_images()` function signature in `tests/visual/image_compare.hpp`
- [x] 3.3 Implement `tests/visual/image_compare.cpp` — per-channel comparison (R/G/B/A independently), diff image generation (failing pixels → red overlay at full intensity, passing pixels → 25% intensity), size-mismatch early return with `error_message`
- [x] 3.4 Implement `tests/visual/image_compare_cli.cpp` — CLI: `goggles_image_compare <actual> <reference> [--tolerance T] [--diff out.png]`; exit 0 = pass, 1 = fail, 2 = file error; print summary line to stdout on failure
- [x] 3.5 Add Catch2 unit tests for `image_compare` in `tests/visual/test_image_compare.cpp` covering: identical images pass, 1-value diff fails at tolerance=0, tolerance allows small diffs, size mismatch fails, diff image is written on failure; register under CTest label `unit`
- [x] 3.6 Build and run unit tests: `ctest --preset test -L unit -R image_compare --output-on-failure`

## 4. Headless Pipeline Smoke Test

- [x] 4.1 Add `add_test(NAME headless_smoke COMMAND ...)` to `tests/CMakeLists.txt` using `$<TARGET_FILE:goggles>` and `$<TARGET_FILE:solid_color_client>` as absolute paths; pass `--frames 5 --output ${CMAKE_CURRENT_BINARY_DIR}/smoke_out.png`
- [x] 4.2 Set `LABELS integration` on the smoke test via `set_tests_properties(headless_smoke PROPERTIES LABELS "integration")`
- [x] 4.3 Add a second `add_test(NAME headless_smoke_png_check ...)` that verifies the PNG was written; set `DEPENDS headless_smoke` via `set_tests_properties`
- [x] 4.4 Run the smoke test: `ctest --preset test -L integration --output-on-failure`; confirm exit 0

## 5. Verification

- [x] 5.1 Confirm `ctest --preset test` passes all tests with no regressions (10/10)
- [x] 5.2 Confirm `ctest --preset test -L unit` runs unit tests including new `image_compare` tests
- [x] 5.3 Confirm `ctest --preset test -L integration` passes the headless smoke test
- [x] 5.4 Confirm `ctest --preset test -L visual` runs zero tests (Phase 2 content not yet added — zero is correct)
- [x] 5.5 Run `pixi run build -p quality` (clang-tidy WarningsAsErrors) against all new source files; fix any clang-tidy violations
- [x] 5.6 Run `pixi run format`; confirm no formatting changes needed
