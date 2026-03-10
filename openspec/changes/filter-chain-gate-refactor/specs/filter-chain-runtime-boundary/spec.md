## ADDED Requirements

### Requirement: Gate-Owned Filter Chain Runtime Boundary
The filter-chain implementation SHALL treat `src/render/chain/api/cpp/goggles_filter_chain.hpp` and
`src/render/chain/api/c/goggles_filter_chain.h` as the only stable integration gates. Internal
runtime, build, resource, and control types SHALL remain behind the C ABI handle and SHALL NOT be
referenced from backend-facing code or the public install surface.

#### Scenario: Backend stays on the stable gate
- **GIVEN** runtime backend code under `src/render/backend/` needs filter-chain services
- **WHEN** the gate-centered refactor is integrated
- **THEN** backend code SHALL interact through `FilterChainRuntime`
- **AND** direct dependence on internal chain runtime/build/resource/control types SHALL be absent

### Requirement: Backend-Owned Vulkan Handoff Seam Remains Intact
`VulkanBackend` SHALL retain ownership of root backend Vulkan state, swapchain/render-output
behavior, external-frame import, and presentation synchronization. The filter-chain runtime SHALL
continue to receive only boundary-scoped Vulkan inputs produced by the backend handoff seam rather
than taking ownership of backend render-output concerns.

#### Scenario: Runtime creation uses boundary-scoped Vulkan inputs only
- **GIVEN** `VulkanBackend` creates or rebuilds a filter-chain runtime
- **WHEN** it prepares the runtime build configuration for the stable gate
- **THEN** the backend SHALL pass only boundary-scoped Vulkan inputs into the filter-chain runtime
- **AND** root backend Vulkan or render-output ownership SHALL remain outside `ChainRuntime`

### Requirement: Async Rebuild Isolation And State-Preserving Swap
The gate-owned runtime SHALL build pending preset/runtime state without mutating the active runtime.
If async rebuild fails, the active runtime SHALL remain usable with its prior preset, effective stage
policy, control values, and prechain configuration. If async rebuild succeeds, the swapped-in runtime
SHALL preserve that state before its first rendered frame.

#### Scenario: Failed rebuild leaves active runtime intact
- **GIVEN** an active filter-chain runtime is rendering frames
- **WHEN** an async preset rebuild fails before swap
- **THEN** the active runtime SHALL continue using its existing preset and effective state
- **AND** no frame SHALL render with partially installed pending state

#### Scenario: Successful swap preserves effective state
- **GIVEN** an active runtime has stage policy, control overrides, and prechain configuration applied
- **WHEN** an async preset rebuild completes and swaps in a new runtime
- **THEN** the new runtime SHALL render its first frame with the same effective policy, control
  values, and prechain configuration

### Requirement: Record Path Uses Prepared Runtime State Only
Once a preset/runtime is active, `record()` SHALL consume prepared compiled state and runtime
resources only. The record path SHALL NOT parse presets, compile shaders, perform filesystem I/O, or
block on background build completion. Record-path execution SHALL preserve stage order
`prechain -> effect -> postchain`.

#### Scenario: Hot path stays bounded
- **GIVEN** a prepared active runtime and a valid frame record request
- **WHEN** `record()` executes for a frame
- **THEN** it SHALL use prepared compiled state and runtime resources only
- **AND** it SHALL preserve `prechain -> effect -> postchain` ordering

### Requirement: Controller Scope Is Orchestration Only
`FilterChainController` SHALL manage active/pending runtime instances, async rebuild submission,
safe swap timing, and retired-runtime cleanup only. Stage-policy interpretation, prechain
resolution normalization, control descriptor generation, and control value semantics SHALL be owned
inside the gate-owned runtime.

#### Scenario: Controller forwards desired semantics without owning them
- **GIVEN** backend code requests policy, resolution, or preset changes
- **WHEN** `FilterChainController` handles those requests
- **THEN** it SHALL forward the desired values into the stable runtime gate
- **AND** filter semantic interpretation SHALL remain inside the gate-owned runtime

### Requirement: Runtime Retirement Has An Explicit Safety Contract
Swapped-out runtimes SHALL remain valid until it is safe to destroy them relative to frames in
flight. The implementation SHALL prefer explicit submission-epoch or fence-backed retirement when the
backend plumbing supports it; otherwise it SHALL use an isolated bounded fallback retirement helper
with dedicated verification coverage.

#### Scenario: Fallback retirement remains bounded and explicit
- **GIVEN** explicit submission/fence-backed retirement is not available yet
- **WHEN** a runtime swap retires the previous runtime instance
- **THEN** destruction SHALL be deferred through one isolated fallback helper
- **AND** verification coverage SHALL name the fallback behavior and its safety assumptions
