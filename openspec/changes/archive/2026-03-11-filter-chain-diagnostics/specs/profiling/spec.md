# Delta for profiling

## ADDED Requirements

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
