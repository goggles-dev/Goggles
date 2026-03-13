# Delta for render-pipeline

## MODIFIED Requirements

### Requirement: Swapchain Format Matching

The render backend SHALL match swapchain output color space to the current source image
color-space classification to preserve pixel values. When the source classification changes without
an explicit preset change request, the pipeline SHALL recreate backend-owned swapchain and
presentation resources, SHALL retarget the filter runtime through the output-retarget seam, and
SHALL preserve source-independent preset-derived state instead of forcing a full preset reload.

#### Scenario: Source color-space change uses retarget seam
- GIVEN a preset is already active and rendering with an SRGB-matched output path
- WHEN the source image classification changes to UNORM
- THEN the backend SHALL recreate the swapchain with a matching UNORM output format
- AND the filter runtime SHALL be retargeted through the output-retarget seam rather than by
  reloading the preset

#### Scenario: Successful retarget preserves preset-derived state
- GIVEN a source color-space change triggers output-format retargeting
- WHEN the retarget succeeds
- THEN the active preset selection, control layout, and parameter overrides SHALL remain unchanged
- AND source-independent preset-derived runtime work SHALL remain available after the transition

### Requirement: Async Filter Lifecycle Safety

The render pipeline SHALL preserve async preset reload, output-format retarget, pending-runtime swap,
and resize safety while Goggles consumes the filter runtime through the stable boundary. Backend-owned
swapchain and present lifecycle SHALL remain separate from boundary-owned output-state rebuild
behavior.

#### Scenario: Pending reload is aligned before swap
- GIVEN an explicit preset reload is building a pending runtime
- AND the authoritative output target changes before that runtime becomes active
- WHEN the pending runtime is prepared for swap
- THEN the pending runtime SHALL be retargeted to the latest output target before activation
- AND the system SHALL NOT swap in a runtime bound to stale output state and immediately retarget it

#### Scenario: Retarget failure keeps prior runtime active
- GIVEN an output-format retarget attempt fails before activation
- WHEN host code checks active runtime state after the failure
- THEN the previously active runtime SHALL remain the active rendering runtime
- AND the failed attempt SHALL NOT force fallback to full preset reload behavior
