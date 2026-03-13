# goggles-filter-chain Specification

## Purpose
Define the boundary contract between the host render/backend code and the standalone
`goggles-filter-chain` runtime, including ownership, lifecycle, controls, and diagnostics.
## Requirements
### Requirement: Standalone Filter Library Target
Within Goggles, the filter runtime SHALL remain consumable through a target named
`goggles-filter-chain` whether the implementation comes from in-tree sources, a local subdirectory
provider, or the monorepo package/install rehearsal path. Goggles host targets SHALL preserve
one-way dependency direction toward that target and SHALL NOT regain direct dependence on concrete
filter runtime implementation headers.

#### Scenario: Stable target contract across Goggles provider modes
- GIVEN Goggles is configured for `in-tree`, `subdir`, or `package` provider resolution
- WHEN render targets are linked
- THEN downstream Goggles targets SHALL consume `goggles-filter-chain` in every mode
- AND consumer CMake logic SHALL NOT require provider-specific target names

#### Scenario: Host dependency direction remains one-way
- GIVEN build dependency checks run for Goggles host and filter runtime targets
- WHEN target relationships are audited
- THEN Goggles host/backend targets SHALL depend on `goggles-filter-chain`
- AND `goggles-filter-chain` SHALL NOT depend on Goggles backend, app, or UI targets

### Requirement: Complete Filter Runtime Ownership Boundary
The filter boundary SHALL own filter-chain orchestration, shader runtime ownership/creation,
shader processing, and preset texture loading internals. When the host retargets output format
without changing the active preset, the boundary SHALL preserve source-independent
preset-derived runtime state across that retarget.

#### Scenario: Source-independent preset work survives output retarget
- **GIVEN** a preset runtime has already completed parsing, shader compilation/reflection, preset
  texture loading, and effect-pass setup
- **WHEN** the host requests output-format retargeting for a source color-space change
- **THEN** that source-independent preset-derived work SHALL remain available after the retarget
- **AND** the boundary SHALL expose the same effect-stage behavior after activation

### Requirement: Host Backend Responsibility Boundary
The host backend SHALL remain responsible for swapchain lifecycle, external image import,
synchronization, queue submission, and present. The filter boundary SHALL own source-independent
preset/runtime state plus output-state rebuild and swap-on-success for retarget. Output-format
changes for an unchanged preset SHALL use the retarget seam and SHALL NOT trigger full
preset/runtime rebuild behavior.

#### Scenario: Format retarget rebuilds boundary-owned output state only
- GIVEN an active preset runtime already owns parsed preset state, controls, textures, and compiled
  shader/runtime state
- WHEN the authoritative output target changes without an explicit preset change
- THEN Goggles host code SHALL recreate host-owned swapchain and presentation resources
- AND the boundary SHALL retarget output-side runtime state without discarding source-independent
  preset state

#### Scenario: Explicit preset reload remains separate from retarget
- GIVEN the user explicitly selects a different preset or requests reload
- WHEN Goggles coordinates the change
- THEN the boundary interaction SHALL use full preset/runtime rebuild behavior
- AND that request SHALL remain distinct from format-only output retargeting

### Requirement: Boundary-owned Public Support Surface

The reusable filter boundary SHALL expose only boundary-owned support, diagnostics, and context
contracts on its public consumer surface. Compatibility forwarders MAY remain in the monorepo, but
public consumer expectations SHALL be defined by the boundary-owned include surface.

#### Scenario: Public surface excludes Goggles-only utility headers
- GIVEN a consumer compiles against the public filter-chain headers used by Goggles
- WHEN include and type dependencies are audited
- THEN the public surface SHALL depend only on boundary-owned declarations plus allowed third-party
  headers
- AND public declarations SHALL NOT require Goggles-only `src/util/*` or backend-private helper
  headers

#### Scenario: Compatibility forwarders stay behind the contract
- GIVEN the monorepo keeps legacy include paths for compatibility
- WHEN those forwarders are present
- THEN they SHALL forward to boundary-owned headers
- AND the stable consumer contract SHALL remain the boundary-owned include surface

### Requirement: Reusable Contract Coverage Boundary

Reusable contract tests SHALL validate boundary APIs, retarget-vs-reload behavior, and public-surface
hygiene through the filter-chain boundary. Goggles-owned host integration tests SHALL validate
backend/controller ownership and presentation wiring without redefining reusable boundary behavior.

#### Scenario: Boundary contract tests remain separable
- GIVEN reusable contract coverage is registered in the monorepo
- WHEN maintainers select the contract test target
- THEN those tests SHALL exercise the filter-chain boundary without requiring Goggles presentation
  behavior to define expected results

#### Scenario: Host integration tests keep backend ownership assertions
- GIVEN Goggles integration coverage asserts swapchain recreation, pending-runtime alignment, or
  presentation behavior
- WHEN those tests run
- THEN that coverage SHALL remain Goggles-owned host behavior
- AND it SHALL complement rather than replace reusable boundary contract coverage

#### Scenario: Pending runtime is aligned before activation
- **GIVEN** the host backend has a pending runtime from an explicit reload
- **AND** the authoritative output target changes before that pending runtime becomes active
- **WHEN** the backend/controller prepares the pending runtime for activation
- **THEN** the boundary interaction SHALL align that pending runtime to the current output target
- **AND** activation SHALL occur only after the pending runtime matches the latest output format

### Requirement: Boundary-safe VulkanContext Contract Placement
Host<->filter initialization contracts SHALL use a boundary-owned `VulkanContext` definition that does not pull backend internals into the filter boundary.

#### Scenario: VulkanContext ownership and include safety
- **GIVEN** headers used to define `VulkanContext` for host<->filter initialization
- **WHEN** include/dependency checks run
- **THEN** the `VulkanContext` type SHALL be declared in a boundary-owned header
- **AND** that header SHALL include only boundary-allowed dependencies and SHALL NOT include backend-only helper headers

#### Scenario: Host/backend consumption of VulkanContext contract
- **GIVEN** backend and filter runtime initialization paths
- **WHEN** host code passes initialization context into `goggles-filter-chain`
- **THEN** host code SHALL consume the boundary-owned `VulkanContext` contract
- **AND** backend public headers SHALL NOT expose filter-boundary internals beyond this contract

### Requirement: Boundary-safe Vulkan Result Utility Contracts
Filter boundary code SHALL use boundary-safe Vulkan result utilities and SHALL NOT include backend-only helper headers.

#### Scenario: Backend helper include removal
- **GIVEN** chain/shader/texture boundary source files
- **WHEN** include dependency checks are executed
- **THEN** backend helper headers SHALL NOT be included from boundary sources

#### Scenario: Boundary-safe `VK_TRY` relocation
- **GIVEN** Vulkan result-checking macros used by boundary code
- **WHEN** boundary-safe utility contracts are applied
- **THEN** boundary call sites SHALL include `VK_TRY` from a boundary-safe helper header
- **AND** that helper header SHALL depend only on boundary-allowed headers

### Requirement: Boundary-safe Control Descriptor Contract
The filter boundary SHALL expose curated control descriptors for both effect and prechain controls with a closed, deterministic stage contract.

#### Scenario: Control descriptor enumeration
- **GIVEN** a preset with effect-stage and prechain controls is loaded
- **WHEN** controls are enumerated through the boundary API
- **THEN** each descriptor SHALL include `control_id`, `stage`, `name`, `current_value`, `default_value`, `min_value`, `max_value`, and `step`
- **AND** descriptors SHALL represent both effect and prechain controls through the same boundary-safe contract

#### Scenario: Stage domain is explicit and closed
- **GIVEN** control descriptors returned by the boundary API
- **WHEN** descriptor stage values are validated
- **THEN** each descriptor `stage` SHALL be one of `prechain` or `effect`
- **AND** unknown stage values SHALL NOT be emitted without an explicit spec update

#### Scenario: Deterministic descriptor ordering
- **GIVEN** the same preset is enumerated repeatedly without control-layout changes
- **WHEN** control descriptors are listed through the boundary API
- **THEN** descriptor order SHALL be deterministic across runs and equivalent reloads
- **AND** ordering SHALL group `prechain` controls before `effect` controls while preserving stable per-stage ordering

#### Scenario: Optional description fallback
- **GIVEN** a control descriptor may omit `description`
- **WHEN** UI renders control metadata
- **THEN** UI SHALL render a control label from `name`
- **AND** UI SHALL apply no tooltip text when `description` is absent

### Requirement: Control Identifier Semantics
The control identifier contract SHALL define uniqueness and stability rules for `control_id`.

#### Scenario: Uniqueness within active preset
- **GIVEN** controls for a loaded preset are enumerated
- **WHEN** the boundary returns control descriptors
- **THEN** each `control_id` SHALL be unique within the active preset

#### Scenario: Stability across equivalent reload
- **GIVEN** the same preset is reloaded without control-layout changes
- **WHEN** controls are enumerated after reload
- **THEN** `control_id` values for matching controls SHALL remain stable across reload

#### Scenario: Different preset layouts
- **GIVEN** a different preset with different control layout is loaded
- **WHEN** controls are enumerated for the new preset
- **THEN** `control_id` values MAY differ from the previous preset

### Requirement: Control Mutation Contract
Control mutation and callback contracts SHALL use `control_id` and SHALL define deterministic out-of-range handling.

#### Scenario: Control mutation is `control_id`-only
- **GIVEN** boundary consumers set or reset control values
- **WHEN** control mutation APIs are invoked
- **THEN** operations SHALL address controls by `control_id`
- **AND** boundary surfaces SHALL NOT expose pass indices as mutation keys

#### Scenario: Out-of-range value handling
- **GIVEN** a control descriptor defines `min_value` and `max_value`
- **WHEN** a set-value request provides a value outside `[min_value, max_value]`
- **THEN** the boundary SHALL clamp the request to the nearest valid bound before applying it
- **AND** subsequent control enumeration SHALL report the clamped `current_value`

### Requirement: Adapter Ownership Isolation
Adapters from shader-internal metadata to curated control descriptors SHALL live behind the `goggles-filter-chain` boundary.

#### Scenario: Adapter dependency boundary
- **GIVEN** backend, app, and UI modules consume boundary descriptors
- **WHEN** control metadata adaptation is performed
- **THEN** adaptation from shader-internal metadata SHALL occur inside `goggles-filter-chain`
- **AND** backend/app/UI modules SHALL NOT include shader-internal metadata types for control enumeration

#### Scenario: Adapter mapping parity across effect and prechain sources
- **GIVEN** control metadata comes from effect-stage and prechain-stage sources with different field availability
- **WHEN** adapters build curated descriptors
- **THEN** both sources SHALL map to the same descriptor schema with documented, deterministic field mapping rules
- **AND** when a source omits `current_value`, adapters SHALL emit `current_value` equal to the effective runtime value (or `default_value` when no runtime override exists)

#### Scenario: UI/include isolation from shader internals
- **GIVEN** non-boundary consumer paths such as `src/ui` and `src/app`
- **WHEN** boundary compliance is validated through tests and source audit
- **THEN** non-boundary consumers SHALL NOT include `render/shader/*` headers for control metadata access

### Requirement: No Concrete FilterChain Type Exposure Outside Boundary
Backend public APIs, app code, and UI code MUST NOT depend on concrete chain headers, concrete `FilterChain*` types, or chain accessors that expose concrete internals.

#### Scenario: Include guard in app and UI
- **GIVEN** source files under `src/app` and `src/ui`
- **WHEN** boundary compliance is validated through tests and source audit
- **THEN** files under `src/app` and `src/ui` SHALL NOT include `render/chain/filter_chain.hpp`

#### Scenario: Backend public header guard
- **GIVEN** backend public headers consumed by app/UI and downstream tests
- **WHEN** boundary compliance is validated through tests and source audit
- **THEN** backend public headers SHALL NOT expose concrete `FilterChain` types or accessors returning concrete chain internals

#### Scenario: Type and accessor guard in app and UI
- **GIVEN** source files under `src/app` and `src/ui`
- **WHEN** boundary compliance is validated through tests and source audit
- **THEN** no direct references to concrete `FilterChain*` SHALL exist in app or UI code
- **AND** app/UI code SHALL NOT call backend chain-accessor methods that expose concrete chain internals

### Requirement: Stable Facade API Groups for Downstream Tests
The stable downstream test surface SHALL be limited to boundary facade groups for lifecycle/preset, frame submission, controls, and prechain/policy operations.

#### Scenario: Downstream test compile surface
- **GIVEN** downstream contract tests compile against the boundary facade
- **WHEN** tests include boundary-facing headers only
- **THEN** tests SHALL be able to exercise lifecycle/preset, frame submission, controls, and prechain/policy operations
- **AND** tests SHALL NOT require concrete chain, pass, shader-runtime, or deferred-destroy internal types

### Requirement: Facade Active-Chain Invocation Safety
Boundary facade methods SHALL resolve active chain/runtime references at call time and SHALL NOT cache concrete chain pointers across calls.

#### Scenario: Async swap with subsequent facade calls
- **GIVEN** an async preset reload swaps in a new active chain/runtime
- **WHEN** subsequent facade operations are invoked
- **THEN** each facade call SHALL target the current active chain/runtime reference
- **AND** facade operations SHALL NOT use stale cached concrete chain pointers from prior calls

### Requirement: Error Model and Diagnostics Contract

All fallible APIs MUST return `goggles_chain_status_t` and MUST NOT surface exceptions as part of the public contract. `goggles_chain_status_to_string(...)` MUST return a stable static string and unknown status values MUST map to `"UNKNOWN_STATUS"`. Structured diagnostics MUST be queryable through `goggles_chain_error_last_info_get(...)` and, when a diagnostic session is active, through the diagnostic session's event and artifact retrieval APIs.

#### Scenario: Diagnostic session supplements last-error
- **GIVEN** a diagnostic session is active and an API call fails
- **WHEN** the host queries diagnostics
- **THEN** `goggles_chain_error_last_info_get(...)` SHALL still return the last error info
- **AND** the diagnostic session SHALL contain a corresponding diagnostic event with richer context including localization and evidence payload

#### Scenario: Diagnostics-not-active status has a stable code
- **GIVEN** a READY runtime with no diagnostic session created
- **WHEN** the host calls a diagnostics-session-only function such as sink registration or summary retrieval
- **THEN** the boundary SHALL return `GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE`
- **AND** `goggles_chain_status_to_string(...)` SHALL map that code to `"DIAGNOSTICS_NOT_ACTIVE"`

### Requirement: Diagnostic Session Lifecycle Through Boundary API

The filter-chain boundary SHALL expose diagnostic session creation, query, and teardown through the boundary-facing contract so that host code can control diagnostic depth without accessing chain internals.

#### Scenario: Host creates diagnostic session with policy
- **GIVEN** a READY filter-chain runtime instance
- **WHEN** the host requests diagnostic session creation with a specified reporting mode and strict/compatibility policy
- **THEN** the boundary SHALL create a diagnostic session scoped to the runtime's lifetime
- **AND** the session SHALL apply the specified policy to all subsequent diagnostic emission

#### Scenario: Host queries diagnostic session state
- **GIVEN** an active diagnostic session on a filter-chain runtime
- **WHEN** the host queries session state through the boundary API
- **THEN** the boundary SHALL return the current reporting mode, policy mode, and aggregate event counts by severity

#### Scenario: No diagnostic session is a valid state
- **GIVEN** a READY filter-chain runtime with no diagnostic session created
- **WHEN** the runtime records frames
- **THEN** the runtime SHALL operate without diagnostics-specific artifacts or sink delivery
- **AND** host-visible diagnostic ledgers and summaries SHALL remain unavailable until a session is created

### Requirement: Sink Registration Through Boundary API

The filter-chain boundary SHALL allow host code to register and unregister diagnostic sink adapters without exposing concrete sink implementation types.

#### Scenario: Host registers a sink adapter
- **GIVEN** a diagnostic session exists on a filter-chain runtime
- **WHEN** the host registers a sink adapter through the boundary API
- **THEN** subsequent diagnostic events SHALL be delivered to the registered sink
- **AND** the registration SHALL return an identifier for later unregistration

#### Scenario: Host unregisters a sink adapter
- **GIVEN** a sink adapter is registered with a diagnostic session
- **WHEN** the host unregisters the sink using its identifier
- **THEN** subsequent diagnostic events SHALL NOT be delivered to the unregistered sink
- **AND** events already in flight SHALL complete delivery

#### Scenario: Multiple sinks from different hosts
- **GIVEN** a diagnostic session with two registered sink adapters
- **WHEN** a diagnostic event is emitted
- **THEN** both sinks SHALL receive the event
- **AND** delivery order SHALL be deterministic (registration order)

### Requirement: Diagnostic Event Retrieval for External Consumers

The filter-chain boundary SHALL expose lightweight diagnostic session summary and callback delivery primitives without requiring external consumers to access runtime internals.

#### Scenario: Host retrieves diagnostic summary counts
- **GIVEN** a diagnostic session is active and events have been emitted
- **WHEN** the host calls `goggles_chain_diagnostics_summary_get(...)`
- **THEN** the boundary SHALL return the active reporting mode, policy mode, and aggregate error, warning, and info counts
- **AND** the call SHALL return `GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE` when no session exists

#### Scenario: Host receives events through callback sink registration
- **GIVEN** a diagnostic session exists on a runtime
- **WHEN** the host registers `goggles_chain_diagnostic_event_cb` through `goggles_chain_diagnostics_sink_register(...)`
- **THEN** subsequent emitted events SHALL be forwarded to that callback with severity, category, pass ordinal, message text, and user data
- **AND** the registration SHALL produce a sink identifier that can be passed to `goggles_chain_diagnostics_sink_unregister(...)`

#### Scenario: Retrieval before preset load
- **GIVEN** a runtime in CREATED state with no preset loaded
- **WHEN** the host requests diagnostic artifacts through the boundary API
- **THEN** the boundary SHALL return a not-initialized status
- **AND** no partial or stale artifacts SHALL be returned

### Requirement: Capture Control Through Boundary API

The filter-chain boundary SHALL reserve future space for capture control, while the current implemented boundary surface stops at session lifecycle, callback sink registration, and summary retrieval.

#### Scenario: Current boundary does not yet expose capture requests
- **GIVEN** a host is using the current C boundary header
- **WHEN** it inspects the exported diagnostics functions
- **THEN** the implemented diagnostics surface SHALL consist of `goggles_chain_diagnostics_session_create(...)`, `goggles_chain_diagnostics_session_destroy(...)`, `goggles_chain_diagnostics_sink_register(...)`, `goggles_chain_diagnostics_sink_unregister(...)`, and `goggles_chain_diagnostics_summary_get(...)`
- **AND** no pass-range or frame-range capture request API SHALL yet be present

### Requirement: Boundary Diagnostic Event Emission Does Not Break Frame Recording Contract

Diagnostic event emission through the boundary SHALL NOT violate the existing frame recording performance contract.

#### Scenario: Tier 0 events during record
- **GIVEN** the filter-chain is recording commands for a frame
- **WHEN** Tier 0 diagnostic events are emitted
- **THEN** no heap allocation, file I/O, shader compilation, or blocking wait SHALL occur in the event emission path

#### Scenario: Tier 1 events with GPU timestamps
- **GIVEN** Tier 1 diagnostics are active during frame recording
- **WHEN** GPU timestamp queries are inserted for pass-level timing
- **THEN** timestamp query commands SHALL be recorded into the same command buffer
- **AND** timestamp readback SHALL occur asynchronously after frame submission, not during recording
