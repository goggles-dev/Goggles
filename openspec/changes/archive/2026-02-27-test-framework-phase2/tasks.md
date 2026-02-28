## 1. Per-test TOML Configs

- [x] 1.1 Create `tests/visual/configs/aspect_fit_letterbox.toml` — `scale_mode = "fit"`, logging warn
- [x] 1.2 Create `tests/visual/configs/aspect_fit_pillarbox.toml` — `scale_mode = "fit"`, logging warn
- [x] 1.3 Create `tests/visual/configs/aspect_fill.toml` — `scale_mode = "fill"`, logging warn
- [x] 1.4 Create `tests/visual/configs/aspect_stretch.toml` — `scale_mode = "stretch"`, logging warn
- [x] 1.5 Create `tests/visual/configs/aspect_integer_1x.toml` — `scale_mode = "integer"`, `integer_scale = 1`, logging warn
- [x] 1.6 Create `tests/visual/configs/aspect_integer_2x.toml` — `scale_mode = "integer"`, `integer_scale = 2`, logging warn
- [x] 1.7 Create `tests/visual/configs/aspect_integer_auto.toml` — `scale_mode = "integer"`, `integer_scale = 0`, logging warn
- [x] 1.8 Create `tests/visual/configs/aspect_dynamic.toml` — `scale_mode = "dynamic"`, logging warn
- [x] 1.9 Create `tests/visual/configs/shader_bypass.toml` — `scale_mode = "fit"`, no preset, logging warn
- [x] 1.10 Create `tests/visual/configs/shader_zfast.toml` — `scale_mode = "fit"`, `preset = "shaders/retroarch/crt/zfast-crt.slangp"`, logging warn

## 2. Aspect Ratio Visual Tests

- [x] 2.1 Implement `tests/visual/test_aspect_ratio.cpp` with 8 Catch2 test cases:
  - `fit letterbox (1920×1080)` — checks side bars at x<240, x≥1680; quadrant content in 1440×1080
  - `fit pillarbox (800×600)` — perfect 4:3 match, no bars; full 800×600 quadrant check
  - `fill (1920×1080)` — center pixel is non-black (content overflows, no bars visible)
  - `stretch (1920×1080)` — full 1920×1080 quadrant check
  - `integer 1x (1920×1080)` — bars around 640×480 content at offset (640,300)
  - `integer 2x (1920×1080)` — bars around 1280×960 content at offset (320,60)
  - `integer auto (1920×1080)` — auto=2, same geometry as 2x
  - `dynamic (1920×1080)` — falls back to fit, same side bars as letterbox
- [x] 2.2 Use `fork`/`execvp`/`waitpid` subprocess helper; RAII `TempFile`; named `constexpr` tolerance constants
- [x] 2.3 Build: `cmake --build --preset test` compiles `test_aspect_ratio` without warnings

## 3. Build System and Pixi Wiring

- [x] 3.1 Extend `tests/visual/CMakeLists.txt` with `test_aspect_ratio` target: links `image_compare` + `Catch2::Catch2WithMain`, compile definitions for `GOGGLES_BINARY`, `QUADRANT_CLIENT_BINARY`, `VISUAL_CONFIGS_DIR`
- [x] 3.2 Register `test_aspect_ratio` CTest test with `LABELS "visual"`, `TIMEOUT 120`, `ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"`
- [x] 3.3 Extend `tests/visual/CMakeLists.txt` with `test_shader_basic` target: links `image_compare` + `Catch2::Catch2WithMain`, compile definitions for `GOGGLES_BINARY`, `QUADRANT_CLIENT_BINARY`, `VISUAL_CONFIGS_DIR`, `GOLDEN_DIR`
- [x] 3.4 Register `test_shader_basic` CTest test with `LABELS "visual"`, `TIMEOUT 120`, `ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"`
- [x] 3.5 Add `[tasks.update-golden]` to `pixi.toml` with `cmd = "bash scripts/task/update-golden.sh"` and `depends-on = ["build"]`

## 4. Shader Tests and Golden Image Infrastructure

- [x] 4.1 Create `tests/golden/` directory
- [x] 4.2 Create `tests/golden/.gitattributes` — Git LFS tracking for `*.png`
- [x] 4.3 Create `tests/golden/README.md` — documents golden generation, update workflow, LFS policy
- [x] 4.4 Create `scripts/task/update-golden.sh` (executable) — captures `shader_bypass_quadrant.png` and `shader_zfast_quadrant.png` via `goggles --headless` with `quadrant_client`
- [x] 4.5 Implement `tests/visual/test_shader_basic.cpp` with 3 Catch2 test cases:
  - `shader bypass` — runs goggles with bypass config, compares to golden within `BYPASS_TOLERANCE=2/255`, `max_failing_pct=0.1%`; SKIP if golden absent
  - `zfast-crt` — runs goggles with zfast config, compares to golden within `ZFAST_TOLERANCE=0.05`, `max_failing_pct=5.0%`; SKIP if golden absent
  - `filter-chain toggle` — dual-run (bypass + zfast configs), validates each against respective golden; SKIP if either golden absent
- [x] 4.6 Build: `cmake --build --preset test` compiles `test_shader_basic` without warnings

## 5. Verification

- [x] 5.1 Confirm `cmake --build --preset test` (72/72 targets) succeeds with no warnings for new sources
- [x] 5.2 Confirm `ctest --preset test -N -L visual` lists exactly `test_aspect_ratio` and `test_shader_basic`
- [x] 5.3 Confirm `ctest --preset test -N -L unit` does NOT include `test_aspect_ratio` or `test_shader_basic`
- [x] 5.4 Confirm `ctest --preset test -N -L integration` does NOT include `test_aspect_ratio` or `test_shader_basic`
