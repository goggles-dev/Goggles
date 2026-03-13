# Delta for render-pipeline

## MODIFIED Requirements

### Requirement: Swapchain Format Matching

The render backend SHALL match swapchain output color space to the current source image
color-space classification to preserve pixel values. When the source classification changes without
an explicit preset change request, the pipeline SHALL recreate only backend-owned swapchain and
presentation resources, SHALL retarget the filter runtime through the installed public boundary,
and SHALL preserve source-independent preset-derived state instead of forcing a full preset reload.

#### Scenario: External package retarget keeps host ownership split
- GIVEN Goggles consumes the filter runtime as an external standalone package
- WHEN the source image classification changes to require a different output format
- THEN Goggles SHALL recreate only host-owned swapchain and presentation resources
- AND the external filter runtime SHALL be retargeted through its public boundary rather than by reloading the preset

#### Scenario: External consumption preserves preset-derived state
- GIVEN Goggles is linked against the standalone package and a preset is already active
- WHEN a format-only retarget succeeds
- THEN the active preset selection, control layout, and parameter overrides SHALL remain unchanged
- AND source-independent preset-derived runtime work SHALL remain available after the transition
