# Delta for render-pipeline

## ADDED Requirements

### Requirement: Reflection Conformance Gate

The render pipeline SHALL enforce a reflection conformance gate after shader compilation that validates the merged pass contract before pass resources are created.

#### Scenario: Strict mode rejects empty reflection
- GIVEN diagnostic policy is set to strict mode
- WHEN a shader pass compiles successfully but reflection produces an empty contract
- THEN the pipeline SHALL reject the pass and emit an error-severity diagnostic event
- AND the pass SHALL NOT be installed into the compiled chain

#### Scenario: Compatibility mode degrades on empty reflection
- GIVEN diagnostic policy is set to compatibility mode
- WHEN a shader pass compiles successfully but reflection produces an empty contract
- THEN the pipeline SHALL install the pass with a degraded marker
- AND a warning-severity diagnostic event SHALL be emitted
- AND the degradation SHALL be recorded in the degradation ledger

#### Scenario: Binding collision detection
- GIVEN a merged reflection contract for a pass
- WHEN two reflected resources claim the same binding slot with different types or layouts
- THEN the conformance gate SHALL reject the pass in strict mode
- AND the conformance gate SHALL emit a diagnostic event identifying both conflicting resources

### Requirement: Diagnostic Instrumentation Points in Shader Flow

The render pipeline SHALL emit diagnostic events at each stage of the shader processing flow to support authoring analysis.

#### Scenario: Preset parsing emits diagnostic event
- GIVEN a preset file is being loaded
- WHEN preset parsing completes (success or failure)
- THEN the pipeline SHALL emit a diagnostic event with category "authoring" containing the normalized preset structure and any parse errors

#### Scenario: Include expansion emits diagnostic event
- GIVEN shader source undergoes include expansion
- WHEN expansion completes for a pass
- THEN the pipeline SHALL emit a diagnostic event recording the include graph depth, cycle detection result, and expansion success or failure

#### Scenario: Stage compilation emits diagnostic event
- GIVEN vertex and fragment stages are compiled for a pass
- WHEN compilation completes for each stage
- THEN the pipeline SHALL emit a diagnostic event per stage containing compilation success, diagnostic messages, timing, and cache-hit status

#### Scenario: Reflection emits diagnostic event
- GIVEN compiled stages undergo reflection
- WHEN reflection completes for a pass
- THEN the pipeline SHALL emit a diagnostic event containing the reflected resource summary and any merge conflicts

#### Scenario: Diagnostic-aware pipeline entry points use explicit optional outputs
- GIVEN diagnostic-aware authoring analysis is enabled during preset load
- WHEN the render pipeline APIs are invoked
- THEN `ChainBuilder::build(...)` SHALL accept an optional `diagnostics::DiagnosticSession* session = nullptr`
- AND `RetroArchPreprocessor::preprocess(const std::filesystem::path& shader_path, diagnostics::SourceProvenanceMap* provenance = nullptr)` SHALL expose optional provenance capture
- AND `ShaderRuntime::compile_retroarch_shader(const std::string& vertex_source, const std::string& fragment_source, const std::string& module_name, diagnostics::CompileReport* report = nullptr)` SHALL expose optional compile-report output

### Requirement: Source Provenance Tracking in Preprocessing

The render pipeline's shader preprocessing stage SHALL maintain a source provenance map that tracks the origin of every line through include expansion and compatibility rewrites.

#### Scenario: Provenance survives include expansion
- GIVEN a shader with nested includes
- WHEN preprocessing completes
- THEN every line in the expanded output SHALL have a provenance entry mapping to an original file path and line number

#### Scenario: Provenance survives compatibility rewrites
- GIVEN a shader that undergoes compatibility text rewrites
- WHEN preprocessing completes
- THEN rewritten lines SHALL retain provenance to the original source location
- AND the provenance entry SHALL indicate that a rewrite transformation was applied

### Requirement: Pass-Level Runtime Diagnostic Events

The render pipeline SHALL emit diagnostic events during per-pass frame recording to support runtime validation.

#### Scenario: Binding plan event per pass
- GIVEN effect passes are being recorded for a frame
- WHEN texture bindings are rebuilt for a pass
- THEN the pipeline SHALL emit a diagnostic event containing the resolved binding plan with resource identities, fallback status, and extents

#### Scenario: Semantic population event per pass
- GIVEN effect passes are being recorded for a frame
- WHEN semantic values are populated for a pass
- THEN the pipeline SHALL emit a diagnostic event containing the semantic assignment summary with destination classifications

#### Scenario: Fallback substitution event
- GIVEN a non-source texture is missing at binding time during frame recording
- WHEN the engine substitutes the source image as a fallback
- THEN the pipeline SHALL emit a diagnostic event with the expected resource identity, the substituted resource identity, and the pass ordinal

## MODIFIED Requirements

### Requirement: Shader Runtime Compilation

The render shader subsystem SHALL compile Slang shaders to SPIR-V at runtime using the Slang API, supporting both HLSL-style native shaders and GLSL-style RetroArch shaders. Compilation SHALL produce structured compile report artifacts consumable by the diagnostics system.

(Previously: Compilation produced SPIR-V and cached results but did not emit structured diagnostic artifacts.)

#### Scenario: Compilation emits structured compile report
- GIVEN a shader is compiled (cache miss)
- WHEN compilation completes
- THEN the shader runtime SHALL produce a structured compile report containing per-stage success, diagnostic messages with source locations, compilation timing, and cache state
- AND the report SHALL be emittable as a diagnostic event

#### Scenario: Cache hit records provenance
- GIVEN a cached `.spv` file exists with matching source hash
- WHEN the shader is requested and loaded from cache
- THEN the compile report SHALL indicate cache-hit status
- AND the report SHALL preserve the source hash for session identity construction

#### Scenario: Runtime signatures match implemented diagnostics hooks
- GIVEN diagnostics-aware compilation is requested for a RetroArch pass
- WHEN the runtime compiles that pass
- THEN the compile entry point SHALL accept separate preprocessed vertex and fragment source strings plus a module name
- AND compile-report emission SHALL remain optional so existing non-diagnostic call sites can pass `nullptr`
