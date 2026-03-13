# Delta for filter-chain-c-api

## ADDED Requirements

### Requirement: Post-Retarget Output Contract

The C ABI SHALL expose output-target retarget behavior as a contract distinct from preset load and
explicit preset reload. A successful retarget for an unchanged preset SHALL preserve active preset
identity, control state, and other source-independent runtime state while rebuilding only output-side
boundary state required for the new host-owned target.

#### Scenario: Retarget preserves source-independent runtime state
- GIVEN a runtime has already completed preset load and is in READY state
- WHEN the host requests output-target retargeting for a format-only change through the C boundary
- THEN the C ABI contract SHALL preserve active preset identity and existing control state on success
- AND the host SHALL NOT need to reload the preset to adopt the new output target

#### Scenario: Explicit preset reload remains a separate rebuild path
- GIVEN a host explicitly requests preset reload through the C boundary
- WHEN the request is processed
- THEN the contract SHALL treat that request as full preset/runtime rebuild behavior
- AND the host SHALL NOT infer reload semantics from format-only retarget behavior

### Requirement: C ABI Public Surface Isolation

The public C ABI header MUST remain consumable by Goggles through boundary-owned C types and allowed
third-party headers only. Public ABI declarations MUST NOT expose Goggles-internal config headers,
Goggles-only utility headers, or backend-private helper types.

#### Scenario: Public header excludes Goggles-only support dependencies
- GIVEN a consumer includes `goggles_filter_chain.h`
- WHEN header dependencies are audited
- THEN the header SHALL provide its public declarations without requiring Goggles config headers or
  Goggles-only utility headers
- AND the consumer SHALL not need backend-private helper headers to compile against the C ABI

#### Scenario: Ownership split stays explicit at the C boundary
- GIVEN a host uses the C ABI to coordinate rendering and output retargeting
- WHEN ownership responsibilities are validated at the boundary
- THEN host-owned swapchain, presentation, and record-time Vulkan handles SHALL remain host
  responsibilities
- AND runtime-owned persistent state plus output-state retarget behavior SHALL remain boundary
  responsibilities
