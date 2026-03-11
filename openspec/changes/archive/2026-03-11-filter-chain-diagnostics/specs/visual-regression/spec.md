# Delta for visual-regression

## ADDED Requirements

### Requirement: Per-Pass Intermediate Output Golden Baselines

The visual regression system SHALL support golden image baselines for selected intermediate pass outputs, not only the final composited output.

#### Scenario: Intermediate golden for a specific pass
- GIVEN a golden reference image exists for pass ordinal 2 of a specific preset
- WHEN the headless pipeline captures intermediate output for pass 2 and compares it to the golden
- THEN the comparison SHALL use the same tolerance and metric infrastructure as final-output comparisons
- AND the comparison result SHALL identify the pass ordinal in its report

#### Scenario: Multiple intermediate goldens per preset
- GIVEN golden reference images exist for passes 0, 2, and 4 of a multi-pass preset
- WHEN the visual regression suite runs for that preset
- THEN each intermediate golden SHALL be compared independently
- AND failures SHALL be reported per pass with pass ordinal in the failure message

#### Scenario: Intermediate golden update workflow
- GIVEN the golden update mechanism (`pixi run update-golden`)
- WHEN intermediate golden generation is requested for a preset
- THEN the tool SHALL capture and store intermediate outputs for the specified pass ordinals
- AND intermediate goldens SHALL follow the naming convention `{preset_name}_pass{ordinal}.png`

#### Scenario: Missing intermediate golden is a skip, not a failure
- GIVEN no intermediate golden reference exists for a pass
- WHEN visual regression attempts to compare intermediate output for that pass
- THEN the test SHALL emit a Catch2 SKIP (not FAIL)
- AND the skip message SHALL direct the user to generate the intermediate golden

### Requirement: Earliest-Divergence Localization

The visual regression system SHALL support locating the earliest intermediate pass whose output diverges from its golden baseline, enabling failure localization to a specific pass rather than only the final output.

#### Scenario: Divergence localized to earliest failing pass
- GIVEN intermediate goldens exist for passes 0 through 4
- WHEN pass 2 intermediate output diverges from its golden but passes 0 and 1 match
- THEN the regression report SHALL identify pass 2 as the earliest divergent pass
- AND the report SHALL note that passes 3 and 4 are downstream of the divergence

#### Scenario: No intermediate goldens available falls back to final-only
- GIVEN no intermediate goldens exist for a preset
- WHEN the final output diverges from its golden
- THEN the regression report SHALL report the final output failure
- AND the report SHALL note that intermediate golden baselines are unavailable for pass-level localization

### Requirement: Temporal Sequence Golden Baselines

The visual regression system SHALL support golden baselines for multi-frame temporal sequences, validating that history and feedback surfaces produce expected results across frames.

#### Scenario: Multi-frame golden sequence
- GIVEN golden reference images exist for frames 1, 3, and 5 of a temporal preset
- WHEN the headless pipeline captures outputs at those frame indices and compares to goldens
- THEN each frame comparison SHALL be independent
- AND failures SHALL report the frame index alongside the comparison metrics

#### Scenario: Feedback surface golden validation
- GIVEN a preset that uses feedback routing and golden references exist for the feedback consumer pass at frames 2 and 4
- WHEN the visual regression suite captures intermediate outputs at those frames
- THEN the comparison SHALL validate that the feedback surface correctly carries the prior frame's pass output

#### Scenario: Temporal golden update workflow
- GIVEN the golden update mechanism is invoked with frame range specification
- WHEN temporal golden generation is requested
- THEN the tool SHALL capture outputs at the specified frame indices
- AND temporal goldens SHALL follow the naming convention `{preset_name}_frame{index}.png` for final outputs and `{preset_name}_pass{ordinal}_frame{index}.png` for intermediates

### Requirement: Semantic-Probe Presets for Contract Testing

The visual regression system SHALL support synthetic semantic-probe presets that test specific contract behaviors such as size semantic correctness, frame counter progression, and parameter isolation.

#### Scenario: Size semantic probe
- GIVEN a synthetic preset designed to encode source size and output size into pixel values
- WHEN the headless pipeline runs the probe at known source and viewport sizes
- THEN the captured output SHALL encode the expected size values
- AND the regression suite SHALL validate the encoded values against expected values rather than using pixel-diff comparison

#### Scenario: Frame counter probe
- GIVEN a synthetic preset designed to encode the frame count modulo a known value into pixel intensity
- WHEN the headless pipeline captures multiple frames
- THEN each frame's output SHALL encode the expected frame count value
- AND temporal progression SHALL be verified across the captured sequence

#### Scenario: Parameter isolation probe
- GIVEN a synthetic preset where a single parameter controls a measurable visual property
- WHEN the parameter is set to two distinct values and both outputs are captured
- THEN only the expected visual property SHALL differ between the two captures
- AND the regression suite SHALL report any unexpected pixel differences outside the expected region

### Requirement: Diff Heatmap Generation

The visual regression system SHALL support generating diff heatmaps that visually highlight regions of divergence between actual and golden images.

#### Scenario: Heatmap generated on comparison failure
- GIVEN a golden comparison that fails
- WHEN heatmap generation is enabled
- THEN a heatmap image SHALL be produced that maps per-pixel error magnitude to a color gradient
- AND the heatmap SHALL use a perceptually uniform color scale from no-error to maximum-error

#### Scenario: Heatmap for intermediate pass failures
- GIVEN an intermediate pass golden comparison that fails
- WHEN heatmap generation is enabled
- THEN the heatmap SHALL be generated for the intermediate pass output
- AND the heatmap file SHALL include the pass ordinal in its filename

## MODIFIED Requirements

### Requirement: Image comparison library

The system SHALL provide a C++ image comparison library at `tests/visual/image_compare.hpp` that compares two PNG images with configurable per-channel tolerance and supports additional perceptual quality metrics.

(Previously: The library supported per-channel tolerance comparison and basic diff image generation. It now also supports structural similarity metrics and region-of-interest comparison.)

#### Scenario: Structural similarity metric
- GIVEN two PNG files of the same dimensions
- WHEN `compare_images()` is called with structural similarity enabled
- THEN `CompareResult` SHALL include a `structural_similarity` field with a value between 0.0 and 1.0
- AND the structural similarity metric SHALL complement per-channel tolerance for perceptual quality assessment

#### Scenario: Region-of-interest comparison
- GIVEN two PNG files and a specified rectangular region of interest
- WHEN `compare_images()` is called with the region specification
- THEN comparison metrics SHALL be computed only within the specified region
- AND `CompareResult.failing_pixels` SHALL count only pixels within the region that exceed tolerance

### Requirement: Golden image update workflow

The system SHALL provide a reproducible mechanism to regenerate golden reference images including intermediate pass goldens and temporal sequence goldens.

(Previously: The update workflow captured only final bypass and zfast goldens. It now supports intermediate and temporal golden generation.)

#### Scenario: update-golden captures intermediate goldens
- GIVEN the project is built and intermediate golden generation is configured
- WHEN `pixi run update-golden` is executed with intermediate pass specification
- THEN golden images for the specified intermediate passes SHALL be captured and stored
- AND intermediate goldens SHALL be stored alongside final goldens with pass-ordinal naming
