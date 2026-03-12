## MODIFIED Requirements

### Requirement: Complete Filter Runtime Ownership Boundary

The filter boundary SHALL own filter-chain orchestration, shader runtime ownership/creation, shader
processing, and preset texture loading internals. When the host retargets output format without
changing the active preset, the boundary SHALL preserve source-independent preset-derived runtime
state across that retarget.

#### Scenario: Source-independent preset work survives output retarget
- **GIVEN** a preset runtime has already completed parsing, shader compilation/reflection, preset
  texture loading, and effect-pass setup
- **WHEN** the host requests output-format retargeting for a source color-space change
- **THEN** that source-independent preset-derived work SHALL remain available after the retarget
- **AND** the boundary SHALL expose the same effect-stage behavior after activation

### Requirement: Host Backend Responsibility Boundary

The host backend SHALL remain responsible for swapchain lifecycle, external image import,
synchronization, queue submission, and present. The host backend SHALL use boundary-facing retarget
behavior for swapchain/output-format changes and SHALL reserve full preset/runtime rebuild behavior
for explicit preset reload requests.

#### Scenario: Format retarget is handed off without full preset rebuild
- **GIVEN** swapchain output format must change because the source color-space classification changed
- **WHEN** host backend recreation is triggered
- **THEN** host backend code SHALL recreate swapchain and present-path resources
- **AND** filter runtime retargeting SHALL be invoked through boundary-facing contracts without
  forcing full preset rebuild behavior

#### Scenario: Explicit preset reload still uses rebuild path
- **GIVEN** the user explicitly requests a preset reload or selects a different preset
- **WHEN** the host backend coordinates the change
- **THEN** the boundary interaction SHALL use the full preset/runtime rebuild path
- **AND** the request SHALL NOT be collapsed into output-format-only retarget behavior

#### Scenario: Pending runtime is aligned before activation
- **GIVEN** the host backend has a pending runtime from an explicit reload
- **AND** the authoritative output target changes before that pending runtime becomes active
- **WHEN** the backend/controller prepares the pending runtime for activation
- **THEN** the boundary interaction SHALL align that pending runtime to the current output target
- **AND** activation SHALL occur only after the pending runtime matches the latest output format
