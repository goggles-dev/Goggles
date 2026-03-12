## MODIFIED Requirements

### Requirement: Swapchain Format Matching

The render backend SHALL match swapchain output color space to the current source image
color-space classification to preserve pixel values. When the source classification changes without
 a preset change request, the pipeline SHALL retarget only swapchain-bound output resources needed
for the new output format and SHALL NOT treat the event as a full preset reload.

#### Scenario: Source color-space change retargets output side
- **GIVEN** a preset is already active and rendering with an SRGB-matched output path
- **WHEN** the source image classification changes to UNORM
- **THEN** the swapchain SHALL be recreated with a matching UNORM output format
- **AND** the output-side runtime resources bound to swapchain presentation SHALL be retargeted for
  the new format

#### Scenario: Output retarget preserves active preset state
- **GIVEN** a source color-space change triggers output-format retargeting
- **WHEN** the retarget succeeds
- **THEN** the active preset selection SHALL remain unchanged
- **AND** existing parameter overrides and control layout SHALL remain unchanged

### Requirement: Runtime Shader Preset Reload

The render pipeline SHALL support rebuilding the RetroArch filter chain at runtime when the
application explicitly requests a new `.slangp` preset or explicitly reloads the current preset.
Explicit preset reload SHALL remain a full preset/runtime rebuild and SHALL remain distinct from
output-format retargeting caused by source color-space changes.

#### Scenario: Explicit preset reload performs full rebuild
- **GIVEN** a preset is active and the application explicitly requests a preset reload
- **WHEN** the render pipeline handles the request
- **THEN** it SHALL perform full preset reload behavior
- **AND** preset parsing, include expansion, shader compilation/reflection, preset texture loading,
  and effect-pass setup SHALL be re-executed before the replacement runtime becomes active

#### Scenario: Output retarget is not an explicit preset reload
- **GIVEN** the active preset path and runtime controls have not changed
- **WHEN** only the source color-space classification changes
- **THEN** the pipeline SHALL NOT report or execute the event as an explicit preset reload
- **AND** the next frame after a successful transition SHALL continue using the same preset-derived
  effect behavior

#### Scenario: Explicit reload failure preserves previous runtime
- **GIVEN** the application explicitly requests a preset reload
- **WHEN** the requested reload fails before replacement activation
- **THEN** the previously active runtime SHALL remain active for rendering
- **AND** the failure SHALL be reported without leaving a partially activated replacement runtime

### Requirement: Async Filter Lifecycle Safety

The render pipeline SHALL preserve async preset reload, output-format retarget, chain swap, and
resize safety behavior after introducing the `goggles-filter-chain` boundary.

#### Scenario: Output retarget completion is observable only after activation
- **GIVEN** an output-format retarget is performed asynchronously
- **WHEN** the retargeted runtime becomes active for rendering
- **THEN** swap-complete notification SHALL be observable only after the retargeted runtime is active
- **AND** consumers SHALL observe the retargeted output path as current state

#### Scenario: Output retarget failure keeps prior runtime active
- **GIVEN** an output-format retarget attempt fails before activation
- **WHEN** host code checks active runtime state and swap-complete state
- **THEN** the previously active runtime SHALL remain the active rendering runtime
- **AND** no swap-complete indication SHALL be emitted for the failed retarget

#### Scenario: Pending reload is retargeted before swap
- **GIVEN** an explicit preset reload is building a pending runtime
- **AND** the authoritative source color-space classification changes before that runtime becomes
  active
- **WHEN** the pending runtime is prepared for swap
- **THEN** the pending runtime SHALL be retargeted to the latest output format before activation
- **AND** the system SHALL NOT swap in a runtime bound to stale output format and immediately retarget
  it afterward

#### Scenario: Retarget does not change eager preset processing semantics
- **GIVEN** a preset has already been processed into an active runtime
- **WHEN** a later source color-space change triggers output-format retargeting
- **THEN** the system SHALL preserve eager preset processing behavior
- **AND** it SHALL NOT defer preset parsing, compilation, or preset-texture preparation until first
  use after the retarget
