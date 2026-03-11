# Delta for goggles-filter-chain

## ADDED Requirements

### Requirement: Diagnostic Session Lifecycle Through Boundary API

The filter-chain boundary SHALL expose diagnostic session creation, query, and teardown through the boundary-facing contract so that host code can control diagnostic depth without accessing chain internals.

#### Scenario: Host creates diagnostic session with policy
- GIVEN a READY filter-chain runtime instance
- WHEN the host requests diagnostic session creation with a specified reporting mode and strict/compatibility policy
- THEN the boundary SHALL create a diagnostic session scoped to the runtime's lifetime
- AND the session SHALL apply the specified policy to all subsequent diagnostic emission

#### Scenario: Host queries diagnostic session state
- GIVEN an active diagnostic session on a filter-chain runtime
- WHEN the host queries session state through the boundary API
- THEN the boundary SHALL return the current reporting mode, policy mode, and aggregate event counts by severity

#### Scenario: No diagnostic session is a valid state
- GIVEN a READY filter-chain runtime with no diagnostic session created
- WHEN the runtime records frames
- THEN the runtime SHALL operate without diagnostics-specific artifacts or sink delivery
- AND host-visible diagnostic ledgers and summaries SHALL remain unavailable until a session is created

### Requirement: Sink Registration Through Boundary API

The filter-chain boundary SHALL allow host code to register and unregister diagnostic sink adapters without exposing concrete sink implementation types.

#### Scenario: Host registers a sink adapter
- GIVEN a diagnostic session exists on a filter-chain runtime
- WHEN the host registers a sink adapter through the boundary API
- THEN subsequent diagnostic events SHALL be delivered to the registered sink
- AND the registration SHALL return an identifier for later unregistration

#### Scenario: Host unregisters a sink adapter
- GIVEN a sink adapter is registered with a diagnostic session
- WHEN the host unregisters the sink using its identifier
- THEN subsequent diagnostic events SHALL NOT be delivered to the unregistered sink
- AND events already in flight SHALL complete delivery

#### Scenario: Multiple sinks from different hosts
- GIVEN a diagnostic session with two registered sink adapters
- WHEN a diagnostic event is emitted
- THEN both sinks SHALL receive the event
- AND delivery order SHALL be deterministic (registration order)

### Requirement: Diagnostic Event Retrieval for External Consumers

The filter-chain boundary SHALL expose lightweight diagnostic session summary and callback delivery primitives without requiring external consumers to access runtime internals.

#### Scenario: Host retrieves diagnostic summary counts
- GIVEN a diagnostic session is active and events have been emitted
- WHEN the host calls `goggles_chain_diagnostics_summary_get(...)`
- THEN the boundary SHALL return the active reporting mode, policy mode, and aggregate error, warning, and info counts
- AND the call SHALL return `GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE` when no session exists

#### Scenario: Host receives events through callback sink registration
- GIVEN a diagnostic session exists on a runtime
- WHEN the host registers `goggles_chain_diagnostic_event_cb` through `goggles_chain_diagnostics_sink_register(...)`
- THEN subsequent emitted events SHALL be forwarded to that callback with severity, category, pass ordinal, message text, and user data
- AND the registration SHALL produce a sink identifier that can be passed to `goggles_chain_diagnostics_sink_unregister(...)`

#### Scenario: Retrieval before preset load
- GIVEN a runtime in CREATED state with no preset loaded
- WHEN the host requests diagnostic artifacts through the boundary API
- THEN the boundary SHALL return a not-initialized status
- AND no partial or stale artifacts SHALL be returned

### Requirement: Capture Control Through Boundary API

The filter-chain boundary SHALL reserve future space for capture control, while the current implemented boundary surface stops at session lifecycle, callback sink registration, and summary retrieval.

#### Scenario: Current boundary does not yet expose capture requests
- GIVEN a host is using the current C boundary header
- WHEN it inspects the exported diagnostics functions
- THEN the implemented diagnostics surface SHALL consist of `goggles_chain_diagnostics_session_create(...)`, `goggles_chain_diagnostics_session_destroy(...)`, `goggles_chain_diagnostics_sink_register(...)`, `goggles_chain_diagnostics_sink_unregister(...)`, and `goggles_chain_diagnostics_summary_get(...)`
- AND no pass-range or frame-range capture request API SHALL yet be present



### Requirement: Boundary Diagnostic Event Emission Does Not Break Frame Recording Contract

Diagnostic event emission through the boundary SHALL NOT violate the existing frame recording performance contract.

#### Scenario: Tier 0 events during record
- GIVEN the filter-chain is recording commands for a frame
- WHEN Tier 0 diagnostic events are emitted
- THEN no heap allocation, file I/O, shader compilation, or blocking wait SHALL occur in the event emission path

#### Scenario: Tier 1 events with GPU timestamps
- GIVEN Tier 1 diagnostics are active during frame recording
- WHEN GPU timestamp queries are inserted for pass-level timing
- THEN timestamp query commands SHALL be recorded into the same command buffer
- AND timestamp readback SHALL occur asynchronously after frame submission, not during recording

## MODIFIED Requirements

### Requirement: Error Model and Diagnostics Contract

All fallible APIs MUST return `goggles_chain_status_t` and MUST NOT surface exceptions as part of the public contract. `goggles_chain_status_to_string(...)` MUST return a stable static string and unknown status values MUST map to `"UNKNOWN_STATUS"`. Structured diagnostics MUST be queryable through `goggles_chain_error_last_info_get(...)` and, when a diagnostic session is active, through the diagnostic session's event and artifact retrieval APIs.

(Previously: Optional structured diagnostics were queried only through `goggles_chain_error_last_info_get(...)` with `GOGGLES_CHAIN_STATUS_NOT_SUPPORTED` when unsupported. The diagnostic session now provides a richer, always-available diagnostic pathway.)

#### Scenario: Diagnostic session supplements last-error
- GIVEN a diagnostic session is active and an API call fails
- WHEN the host queries diagnostics
- THEN `goggles_chain_error_last_info_get(...)` SHALL still return the last error info
- AND the diagnostic session SHALL contain a corresponding diagnostic event with richer context including localization and evidence payload

#### Scenario: Diagnostics-not-active status has a stable code
- GIVEN a READY runtime with no diagnostic session created
- WHEN the host calls a diagnostics-session-only function such as sink registration or summary retrieval
- THEN the boundary SHALL return `GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE`
- AND `goggles_chain_status_to_string(...)` SHALL map that code to `"DIAGNOSTICS_NOT_ACTIVE"`
