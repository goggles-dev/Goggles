## ADDED Requirements

### Requirement: Visual test targets build unconditionally
The build system SHALL build all visual test clients and the image comparison library as part of the default build, since all dependencies (wayland-client, wayland-protocols, stb_image, Catch2) are already project requirements.

#### Scenario: Default build includes visual targets
- **GIVEN** a clean CMake configuration using any preset
- **WHEN** the build completes
- **THEN** all test client binaries (`solid_color_client`, `gradient_client`, `quadrant_client`, `multi_surface_client`) SHALL be built
- **AND** `goggles_image_compare` CLI binary SHALL be built
- **AND** the `image_compare` static library SHALL be built
- **AND** `test_image_compare` Catch2 test binary SHALL be built

### Requirement: CTest label taxonomy
The build system SHALL register test targets under a consistent label taxonomy using `set_tests_properties(... LABELS ...)`.

#### Scenario: Unit label unchanged
- **WHEN** `ctest -L unit` is run with any preset
- **THEN** existing Catch2 unit tests and `image_compare_unit_tests` SHALL run
- **AND** no integration tests SHALL be included

#### Scenario: Integration label includes headless smoke
- **WHEN** `ctest -L integration` is run with any preset
- **THEN** the headless pipeline smoke test (`headless_smoke`, `headless_smoke_png_check`) SHALL be included
- **AND** existing integration tests (e.g., `auto_input_forwarding`, when available) SHALL also be included

#### Scenario: Visual label for visual regression tests
- **WHEN** `ctest -L visual` is run with any preset
- **THEN** only visual regression test targets (Phase 2+) SHALL run
- **AND** unit and integration tests SHALL NOT be included unless also labeled `visual`
