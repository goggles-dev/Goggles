## ADDED Requirements

### Requirement: Aspect ratio visual tests
The system SHALL provide 8 Catch2 visual tests in `tests/visual/test_aspect_ratio.cpp` that assert correct pixel-region geometry for each scale mode using `quadrant_client` as the fixed 640×480 source.

#### Scenario: fit — source narrower than viewport (letterbox)
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "fit"` and a 640×480 source
- **WHEN** the output PNG is captured
- **THEN** pixels at x<240 and x≥1680 SHALL be black (pillarbox side bars)
- **AND** the content rectangle [240, 0, 1440, 1080] SHALL contain the correct quadrant colors within `CONTENT_TOLERANCE = 2/255`

#### Scenario: fit — source aspect ratio matches viewport (no bars)
- **GIVEN** goggles runs headless at 800×600 with `scale_mode = "fit"` and a 640×480 source
- **WHEN** the output PNG is captured
- **THEN** no black bars SHALL be present
- **AND** the full 800×600 output SHALL contain the correct quadrant colors

#### Scenario: fill — content overflows viewport
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "fill"` and a 640×480 source
- **WHEN** the output PNG is captured
- **THEN** the center pixel (960, 540) SHALL NOT be black

#### Scenario: stretch — entire viewport covered
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "stretch"` and a 640×480 source
- **WHEN** the output PNG is captured
- **THEN** the full 1920×1080 output SHALL contain the correct quadrant colors with no black bars

#### Scenario: integer scale 1x
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "integer"`, `integer_scale = 1`
- **WHEN** the output PNG is captured
- **THEN** the content rectangle SHALL be 640×480 at offset (640, 300)
- **AND** black border pixels SHALL be present in all four surrounding regions

#### Scenario: integer scale 2x
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "integer"`, `integer_scale = 2`
- **WHEN** the output PNG is captured
- **THEN** the content rectangle SHALL be 1280×960 at offset (320, 60)
- **AND** black border pixels SHALL be present in all four surrounding regions

#### Scenario: integer scale auto
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "integer"`, `integer_scale = 0` (auto)
- **WHEN** the output PNG is captured
- **THEN** auto scale SHALL resolve to 2 (min(1920÷640, 1080÷480) = min(3,2) = 2)
- **AND** geometry SHALL match the integer 2x scenario

#### Scenario: dynamic scale mode
- **GIVEN** goggles runs headless at 1920×1080 with `scale_mode = "dynamic"` and a 640×480 source
- **WHEN** the source resolution is stable (no mid-stream change)
- **THEN** dynamic SHALL fall back to fit behaviour
- **AND** geometry SHALL match the fit letterbox scenario (side bars at x<240, x≥1680)

### Requirement: Shader visual tests with golden image comparison
The system SHALL provide 3 Catch2 visual tests in `tests/visual/test_shader_basic.cpp` that compare rendered output to golden reference images within explicit tolerance contracts.

#### Scenario: bypass shader matches golden
- **GIVEN** golden `tests/golden/shader_bypass_quadrant.png` exists
- **WHEN** goggles runs headless with no shader preset and the output is compared to the golden
- **THEN** the comparison SHALL pass with `tolerance = 2/255` and `max_failing_pct ≤ 0.1%`

#### Scenario: zfast-crt shader matches golden
- **GIVEN** golden `tests/golden/shader_zfast_quadrant.png` exists
- **WHEN** goggles runs headless with `zfast-crt.slangp` and the output is compared to the golden
- **THEN** the comparison SHALL pass with `tolerance = 0.05` and `max_failing_pct ≤ 5.0%`

#### Scenario: filter-chain toggle produces distinct bypass and zfast outputs
- **GIVEN** both golden images exist
- **WHEN** goggles is run twice — once with bypass config and once with zfast config
- **THEN** the bypass run SHALL match the bypass golden within bypass tolerance
- **AND** the zfast run SHALL match the zfast golden within zfast tolerance

#### Scenario: tests skip gracefully when goldens are absent
- **GIVEN** golden images have not been generated (e.g. fresh checkout without GPU)
- **WHEN** CTest runs the shader visual tests
- **THEN** each test SHALL emit a Catch2 SKIP (not FAIL) with a message directing the user to run `pixi run update-golden`

### Requirement: Golden image update workflow
The system SHALL provide a reproducible mechanism to regenerate golden reference images.

#### Scenario: update-golden script captures both goldens
- **GIVEN** the project is built (`pixi run build` completed)
- **WHEN** `pixi run update-golden` is executed on a machine with a GPU
- **THEN** `tests/golden/shader_bypass_quadrant.png` SHALL be overwritten with the current bypass render
- **AND** `tests/golden/shader_zfast_quadrant.png` SHALL be overwritten with the current zfast render

#### Scenario: golden PNGs are tracked via Git LFS
- **GIVEN** Git LFS is configured in the repository
- **WHEN** `*.png` files are committed to `tests/golden/`
- **THEN** they SHALL be stored as LFS pointers per `tests/golden/.gitattributes`

### Requirement: Visual test CTest label isolation
Visual tests SHALL carry only the `visual` CTest label; they MUST NOT carry `unit` or `integration` labels.

#### Scenario: visual label selects new tests
- **WHEN** `ctest --preset test -L visual` is run
- **THEN** `test_aspect_ratio` and `test_shader_basic` SHALL be included

#### Scenario: unit label excludes visual tests
- **WHEN** `ctest --preset test -L unit` is run
- **THEN** `test_aspect_ratio` and `test_shader_basic` SHALL NOT be included

#### Scenario: integration label excludes visual tests
- **WHEN** `ctest --preset test -L integration` is run
- **THEN** `test_aspect_ratio` and `test_shader_basic` SHALL NOT be included
