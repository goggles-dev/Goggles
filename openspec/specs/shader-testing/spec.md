# shader-testing Specification

## Purpose
TBD - created by archiving change add-shader-batch-test. Update Purpose after archive.
## Requirements
### Requirement: Batch shader preset testing
The system SHALL provide a batch testing tool that validates all RetroArch shader presets for parsing and compilation compatibility.

#### Scenario: Run batch test on all presets
Given the shaders/retroarch directory contains .slangp files
When `scripts/test_shaders.sh` is executed
Then each preset is tested for parse and compile success
And results are written to build/shader_test_results.json
And exit code is 0 if no regressions, non-zero otherwise

#### Scenario: Filter by category
Given a category argument is provided (e.g., "crt")
When `scripts/test_shaders.sh crt` is executed
Then only presets under shaders/retroarch/crt/ are tested

### Requirement: Machine-readable results

The batch test tool SHALL output results in JSON format containing per-preset status, summary statistics, structured compile reports, reflection summaries, authoring verdicts, and conformance findings.

#### Scenario: JSON output format with diagnostics
- GIVEN batch tests have completed
- WHEN results are written to build/shader_test_results.json
- THEN each preset entry SHALL include: path, parse_ok, compile_ok, error (if any), authoring_verdict, compile_report (per-stage diagnostics), reflection_summary, and conformance_findings
- AND summary SHALL include: total, passed, failed, skipped, degraded counts

### Requirement: Structured Compile Report in Batch Testing

The batch shader testing tool SHALL produce structured compile reports per preset that include per-stage compilation diagnostics with source-mapped locations, reflection summaries, and authoring verdicts.

#### Scenario: Compile report includes source-mapped diagnostics
- GIVEN a preset with a shader that produces compilation warnings or errors
- WHEN the batch test processes the preset
- THEN the per-preset result SHALL include a compile report with diagnostic messages mapped to original source file paths and line numbers via the source provenance map

#### Scenario: Compile report includes reflection summary
- GIVEN a preset with shaders that compile successfully
- WHEN the batch test processes the preset
- THEN the per-preset result SHALL include a reflection summary listing reflected resources (uniform buffer members, push constant members, texture bindings) per stage per pass

#### Scenario: Compile report includes authoring verdict
- GIVEN a preset has been processed through parsing, compilation, and reflection
- WHEN the batch test generates the per-preset result
- THEN the result SHALL include an authoring verdict (pass, degraded, or fail) based on the diagnostics system's static validation

### Requirement: Authoring Validation Corpus

The shader testing infrastructure SHALL maintain categorized test presets that exercise authoring validation rules, including intentionally invalid presets.

#### Scenario: Invalid preset corpus for parse failures
- GIVEN a set of intentionally malformed preset files
- WHEN batch testing runs the authoring validation corpus
- THEN each malformed preset SHALL produce a "fail" authoring verdict
- AND the diagnostic findings SHALL identify the specific parse failure

#### Scenario: Invalid shader corpus for compile failures
- GIVEN a set of presets referencing intentionally invalid shader sources
- WHEN batch testing runs the authoring validation corpus
- THEN each preset SHALL produce a "fail" authoring verdict with compilation-stage errors
- AND error messages SHALL include source-mapped locations

#### Scenario: Reflection-loss corpus
- GIVEN a set of presets where compilation succeeds but reflection produces incomplete contracts
- WHEN batch testing runs the authoring validation corpus in strict mode
- THEN each preset SHALL produce a "fail" authoring verdict
- AND the findings SHALL identify the specific passes with reflection loss

#### Scenario: Valid presets in corpus still pass
- GIVEN the existing set of valid RetroArch presets
- WHEN batch testing runs the authoring validation corpus
- THEN all previously passing presets SHALL continue to produce "pass" authoring verdicts

### Requirement: Semantic and Parameter Conformance Reporting in Batch Testing

The batch shader testing tool SHALL produce parameter and semantic conformance reports that identify unresolved overrides, duplicate parameter names, and unresolved semantic destinations.

#### Scenario: Unused override detection
- GIVEN a preset with numeric overrides that do not match any reflected parameter destination
- WHEN batch testing processes the preset
- THEN the conformance report SHALL flag each unused override with the override name and the preset source location

#### Scenario: Duplicate parameter name detection
- GIVEN a preset where multiple passes declare parameters with the same name
- WHEN batch testing processes the preset
- THEN the conformance report SHALL flag each duplicate parameter name with the pass ordinals involved

#### Scenario: Unresolved semantic destination detection
- GIVEN a pass with reflected uniform buffer members that do not match any known semantic family
- WHEN batch testing processes the preset
- THEN the conformance report SHALL flag each unresolved destination with the member name, pass ordinal, and suggested near-match if one exists
