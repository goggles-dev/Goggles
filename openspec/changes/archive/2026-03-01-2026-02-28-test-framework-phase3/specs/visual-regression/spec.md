## ADDED Requirements

### Requirement: GPU state validation test suite
The system SHALL provide GPU-state validation tests under `tests/gpu/` that capture headless frames and assert pipeline health using RenderDoc tooling.

#### Scenario: Validation-layer cleanliness assertion
- **GIVEN** a deterministic headless capture generated from a known test client
- **WHEN** `tests/gpu/test_validation_layers.py` runs `rdc assert-clean --min-severity HIGH`
- **THEN** the assertion SHALL pass with zero HIGH-severity validation issues

#### Scenario: Shader pipeline state assertion
- **GIVEN** a capture produced with zfast-crt enabled
- **WHEN** `tests/gpu/test_shader_state.py` asserts pipeline state with `rdc assert-state <event-id> topology --expect TriangleList`
- **THEN** required shader/pipeline characteristics for zfast-crt rendering SHALL be present

#### Scenario: Pixel coordinate assertion
- **GIVEN** a deterministic capture from quadrant-based test content
- **WHEN** `tests/gpu/test_pixel_accuracy.py` executes `rdc assert-pixel` at fixed coordinates
- **THEN** sampled values SHALL match expected color values within configured tolerance

#### Scenario: Failure output includes capture location
- **GIVEN** any GPU-state assertion fails
- **WHEN** pytest reports the failure
- **THEN** output SHALL include the failing `.rdc` capture path
- **AND** output SHALL include a `Command:` line with the failing assertion command/context

### Requirement: Two-frame diff regression assertions
The system SHALL provide `rdc diff` frame-diff assertions to validate expected visual change and expected stability across adjacent captures.

#### Scenario: Shader toggle produces measurable diff
- **GIVEN** one capture without shader and one capture with shader enabled
- **WHEN** `tests/gpu/test_frame_diff.py` executes `rdc diff <baseline.rdc> <candidate.rdc> --framebuffer --json --threshold 0 --diff-output <diff.png>`
- **THEN** measured diff SHALL be greater than 1%

#### Scenario: Parameter change produces localized non-zero diff
- **GIVEN** two captures with only one shader parameter changed
- **WHEN** `rdc diff` metrics are computed
- **THEN** measured diff SHALL be non-zero
- **AND** the change SHALL be localized to expected regions rather than full-frame noise

#### Scenario: Static scene remains stable
- **GIVEN** two consecutive captures of a static scene with identical configuration
- **WHEN** `tests/gpu/test_frame_diff.py` computes `rdc diff` metrics
- **THEN** measured diff SHALL be less than 0.01%

### Requirement: GPU suite execution entrypoint
The system SHALL expose a repeatable command entrypoint for the GPU validation suite.

#### Scenario: GPU suite runs via pixi task
- **GIVEN** GPU test dependencies are available
- **WHEN** `pixi run test-gpu` is executed
- **THEN** pytest SHALL execute tests under `tests/gpu/`
- **AND** command exit status SHALL reflect test pass/fail outcome
