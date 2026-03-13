# Delta for filter-chain-cpp-wrapper

## ADDED Requirements

### Requirement: Wrapper Retarget Contract Mirrors Post-Retarget ABI

The C++ wrapper SHALL expose a typed output-retarget operation that mirrors the post-retarget C ABI
contract. Successful format-only retarget SHALL preserve active preset identity and source-
independent runtime state, while explicit preset reload APIs SHALL remain the full rebuild path.

#### Scenario: Wrapper retarget preserves preset state
- GIVEN backend C++ code holds a wrapper-owned runtime with an active preset
- WHEN it requests output-target retargeting through the wrapper for a format-only change
- THEN the wrapper contract SHALL preserve active preset identity and existing control state on
  success
- AND the callsite SHALL not need to express the change as preset reload

#### Scenario: Wrapper explicit reload remains rebuild behavior
- GIVEN backend C++ code explicitly reloads the current preset or selects a different preset
- WHEN it uses wrapper reload APIs
- THEN the wrapper SHALL expose that request as full preset/runtime rebuild behavior
- AND the wrapper SHALL keep that path distinct from output-format retargeting

### Requirement: Extraction-safe Wrapper Public Surface

The wrapper public surface SHALL depend only on the C ABI, boundary-owned public headers, and `vk::`
types needed by Goggles C++ consumers. Wrapper headers SHALL NOT expose Goggles config types,
Goggles-only utility headers, or backend-private implementation types as part of the consumer
contract.

#### Scenario: Goggles backend stays on the wrapper boundary
- GIVEN Goggles backend integration consumes the filter runtime
- WHEN source-audit checks validate integration boundaries
- THEN backend code SHALL continue to use the wrapper-facing contract for lifecycle and retarget
  behavior
- AND backend code SHALL NOT regain direct dependence on concrete chain internals

#### Scenario: Wrapper header uses boundary-owned support contracts
- GIVEN a consumer includes `goggles_filter_chain.hpp`
- WHEN include dependencies are audited
- THEN the wrapper header SHALL rely on the C ABI plus boundary-owned support headers
- AND the wrapper header SHALL NOT require `util/config.hpp`, `util/error.hpp`, or backend-private
  headers as public dependencies
