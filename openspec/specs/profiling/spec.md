# profiling Specification

## Purpose
Defines Tracy instrumentation and the single-process profile session workflow used by Goggles.
## Requirements
### Requirement: Profiling Infrastructure

The system SHALL provide a compile-time toggleable profiling infrastructure via a CMake option `ENABLE_PROFILING`.

#### Scenario: Profiling disabled (default)

- **WHEN** `ENABLE_PROFILING` is `OFF` (default)
- **THEN** all profiling macros SHALL expand to no-ops
- **AND** no Tracy symbols or overhead SHALL be present in the binary

#### Scenario: Profiling enabled

- **WHEN** `ENABLE_PROFILING` is `ON`
- **THEN** profiling macros SHALL emit Tracy instrumentation
- **AND** the application SHALL be connectable to the Tracy profiler UI

### Requirement: Profiling Macro API

The system SHALL provide profiling macros in `src/util/profiling.hpp` that abstract the underlying profiler implementation.

#### Scenario: Scoped zone profiling

- **WHEN** `GOGGLES_PROFILE_SCOPE(name)` is used
- **THEN** a named profiling zone SHALL be created that ends when the scope exits

#### Scenario: Function profiling

- **WHEN** `GOGGLES_PROFILE_FUNCTION()` is used
- **THEN** a profiling zone named after the enclosing function SHALL be created

#### Scenario: Frame boundary marking

- **WHEN** `GOGGLES_PROFILE_FRAME(name)` is used
- **THEN** a frame boundary marker SHALL be emitted for frame-rate analysis

#### Scenario: Manual zone control

- **WHEN** `GOGGLES_PROFILE_BEGIN(name)` and `GOGGLES_PROFILE_END()` are used as a pair
- **THEN** a profiling zone SHALL span from BEGIN to END

#### Scenario: Zone annotation

- **WHEN** `GOGGLES_PROFILE_TAG(text)` is used within a zone
- **THEN** the zone SHALL be annotated with the provided text

#### Scenario: Numeric value plotting

- **WHEN** `GOGGLES_PROFILE_VALUE(name, value)` is used
- **THEN** the numeric value SHALL be recorded for time-series visualization

### Requirement: Render Pipeline Instrumentation

The system SHALL instrument performance-critical render pipeline functions with profiling zones.

#### Scenario: Frame render profiling

- **WHEN** `VulkanBackend::render_frame()` executes
- **THEN** profiling data SHALL capture the full frame render duration

#### Scenario: Filter chain profiling

- **WHEN** `FilterChain::record()` executes
- **THEN** profiling data SHALL capture the filter chain execution duration

#### Scenario: Per-pass profiling

- **WHEN** `FilterPass::record()` executes
- **THEN** profiling data SHALL capture individual pass durations with pass identification

### Requirement: Shader Compilation Instrumentation

The system SHALL instrument shader compilation functions with profiling zones.

#### Scenario: Shader compilation profiling

- **WHEN** `ShaderRuntime::compile_shader()` executes
- **THEN** profiling data SHALL capture compilation duration

#### Scenario: Cache operation profiling

- **WHEN** SPIR-V cache load or save operations execute
- **THEN** profiling data SHALL capture I/O duration

### Requirement: Compositor Capture Instrumentation

The system SHALL instrument compositor frame export with minimal profiling so frame acquisition remains visible in the viewer trace.

#### Scenario: Compositor frame export profiling

- **WHEN** compositor frame preparation and DMA-BUF export execute
- **THEN** profiling data SHALL capture that frame acquisition work in the viewer trace

### Requirement: CMake Build Preset

The system SHALL provide a CMake preset for profiling builds.

#### Scenario: Profile preset usage

- **WHEN** building with `cmake --preset profile`
- **THEN** a Release build with profiling enabled SHALL be produced

### Requirement: Unified Profile Session Command

The system SHALL provide a profile-session command that orchestrates build, launch, capture, and
artifact generation for a Goggles viewer profile session.

#### Scenario: Start-pattern CLI for profile sessions

- **WHEN** a user runs `pixi run profile [goggles_args...] -- <app> [app_args...]`
- **THEN** the command SHALL parse arguments using the same split semantics as `pixi run start`
- **AND** it SHALL launch a profiling session without requiring manual Tracy command orchestration

### Requirement: Viewer Trace Capture

A profile session SHALL capture the Goggles viewer/compositor process and persist one raw trace.

#### Scenario: Capture viewer trace in one run

- **WHEN** a profile session completes successfully
- **THEN** it SHALL produce `viewer.tracy` for the viewer/compositor process

#### Scenario: Capture worker log is preserved

- **WHEN** a profile session starts Tracy capture for the viewer process
- **THEN** it SHALL persist the capture worker output alongside the session artifacts

### Requirement: Session Artifact Manifest

The system SHALL persist machine-readable metadata for every profile session.

#### Scenario: Manifest contains reproducibility metadata

- **WHEN** a session finishes
- **THEN** it SHALL write a manifest describing command line, timestamps, ports, client mapping, and artifact paths
- **AND** it SHALL include exit codes and warnings encountered during capture

### Requirement: Per-Pass GPU Timestamp Queries

The profiling system SHALL support per-pass GPU timestamp queries that measure actual GPU execution time for each filter pass, complementing existing Tracy CPU profiling.

#### Scenario: GPU timestamps recorded per effect pass
- GIVEN Tier 1 diagnostics are active and the GPU supports timestamp queries
- WHEN effect passes are recorded for a frame
- THEN a GPU timestamp query SHALL be written before and after each pass's draw commands
- AND the timestamp pair SHALL be associated with the pass ordinal

#### Scenario: GPU timestamps for pre-processing and final composition
- GIVEN Tier 1 diagnostics are active
- WHEN the pre-processing region and final composition region are recorded
- THEN GPU timestamp queries SHALL bracket these regions as well
- AND timestamps SHALL be distinguishable from effect-pass timestamps by region identifier

#### Scenario: GPU timestamp readback is asynchronous
- GIVEN GPU timestamp queries have been written during frame recording
- WHEN the frame submission completes
- THEN timestamp results SHALL be read back asynchronously (after fence signal)
- AND readback SHALL NOT stall the next frame's recording

#### Scenario: GPU timestamps unavailable
- GIVEN the physical device reports `timestampPeriod` of zero or does not support timestamp queries
- WHEN Tier 1 diagnostics are requested
- THEN GPU timestamp collection SHALL be silently disabled
- AND a diagnostic info-severity event SHALL note that GPU timestamps are unavailable
- AND other Tier 1 diagnostics SHALL continue to function

### Requirement: GPU Timing Integration with Execution Timeline

Per-pass GPU timestamp results SHALL be integrated into the diagnostic execution timeline to provide a unified view of CPU and GPU pass durations.

#### Scenario: Execution timeline includes GPU timing
- GIVEN Tier 1 diagnostics are active and GPU timestamps are available
- WHEN the execution timeline is generated for a frame
- THEN each pass event in the timeline SHALL include both CPU-side timing (from Tracy or wall clock) and GPU-side timing (from timestamp queries)

#### Scenario: GPU timing identifies bottleneck pass
- GIVEN a frame with multiple effect passes and GPU timestamps
- WHEN the execution timeline is analyzed
- THEN the pass with the longest GPU duration SHALL be identifiable from the timeline data
- AND the timeline SHALL support sorting or ranking passes by GPU duration

### Requirement: Profiling Debug Labels

The profiling system SHALL insert Vulkan debug labels at pass boundaries so that GPU profiling tools and validation layers can associate work with specific passes.

#### Scenario: Debug labels inserted per pass
- GIVEN debug label support is available (VK_EXT_debug_utils)
- WHEN effect passes are recorded
- THEN a debug label SHALL be inserted before each pass's draw commands identifying the pass by ordinal and shader name
- AND the label SHALL be ended after the pass's draw commands

#### Scenario: Debug labels for temporal operations
- GIVEN debug label support is available
- WHEN history push and feedback copy operations are recorded
- THEN debug labels SHALL bracket these operations with descriptive names

#### Scenario: Debug labels disabled without extension
- GIVEN the Vulkan instance or device does not support VK_EXT_debug_utils
- WHEN pass recording occurs
- THEN no debug label insertion SHALL be attempted
- AND no runtime error SHALL occur
