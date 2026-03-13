# filter-chain-cpp-wrapper Specification

## Purpose
TBD - created by archiving change cpp20-filter-chain-hpp-wrapper. Update Purpose after archive.
## Requirements
### Requirement: C++20 Wrapper Header for Filter Chain

The project SHALL provide a C++20 header for filter-chain runtime integration that wraps the C ABI in `src/render/chain/include/goggles_filter_chain.h` for C++ consumers.

#### Scenario: Wrapper header exists in public include surface
- **GIVEN** a wave-1 runtime C++ module integrates filter-chain operations
- **WHEN** a C++ runtime module needs filter-chain integration
- **THEN** it SHALL consume the C++ wrapper header instead of directly including `goggles_filter_chain.h`

### Requirement: RAII Ownership for Chain Handle

The C++ wrapper SHALL expose RAII ownership for the runtime chain handle so C++ callsites do not manage pointer-to-pointer destruction flows.

#### Scenario: Handle destruction
- **GIVEN** a wrapper-owned chain instance holds a valid underlying C ABI handle
- **WHEN** a wrapper-owned chain instance goes out of scope
- **THEN** the underlying C ABI chain handle SHALL be destroyed exactly once
- **AND** no explicit pointer-to-pointer destroy call SHALL be required by normal C++ callsites

### Requirement: Strongly Typed C++ Runtime Surface

The C++ wrapper SHALL provide strongly typed C++ interfaces for wave-1 operations (create, preset load, resize, record, stage policy), and SHALL NOT expose C-style macro/size ceremony in normal usage.

#### Scenario: Runtime operation callsite shape
- **GIVEN** wave-1 wrapper lifecycle/runtime APIs are available to runtime backend code
- **WHEN** a C++ runtime caller invokes wave-1 filter-chain operations
- **THEN** the callsite SHALL use typed C++ arguments/results
- **AND** it SHALL NOT require `struct_size` initialization macros, raw status-code switch logic, or pointer-to-pointer out-parameter ownership patterns

### Requirement: Result-Based Error Propagation

Every fallible wrapper operation SHALL use project `Result` (`tl::expected<T, Error>` or project alias) and SHALL NOT require exceptions for expected runtime failures.

#### Scenario: Wrapper failure path
- **GIVEN** a wrapper operation reaches an expected runtime-failure condition
- **WHEN** a wrapper operation encounters an expected runtime failure
- **THEN** it SHALL return a failed project `Result`
- **AND** it SHALL NOT throw exceptions for that expected failure

### Requirement: Single-Boundary Error Logging

Wrapper migration paths SHALL ensure each error is handled or propagated, and duplicate cascading logs across wrapper/runtime stack layers SHALL NOT occur.

#### Scenario: Error crosses wrapper/runtime boundary
- **GIVEN** a lower-layer error is propagated through wrapper/runtime call paths
- **WHEN** a wrapper operation fails and the error is propagated to a subsystem boundary
- **THEN** the error SHALL emit exactly one boundary log event with actionable context
- **AND** lower layers SHALL emit zero duplicate logs for the same failure event

### Requirement: App-Side Vulkan Type Conventions

Wrapper-facing app/runtime C++ signatures SHALL use `vk::` types and SHALL NOT expose raw `Vk*` handles to C++ consumers.

#### Scenario: Wrapper API signature conventions
- **GIVEN** wrapper-facing app/runtime API declarations are authored for wave 1
- **WHEN** wave-1 wrapper interfaces are declared
- **THEN** app/runtime-facing Vulkan parameters and return values SHALL use `vk::` conventions
- **AND** raw `Vk*` handle types SHALL remain confined to C ABI boundary internals
- **AND** wrapper-facing public headers SHALL contain no raw `Vk*` parameter or return signatures

### Requirement: Boundary Isolation

The C header SHALL remain the ABI boundary contract only. Internal runtime C++ modules in wave-1 scope SHALL NOT directly include or call the C ABI header, and wrapper adoption SHALL avoid re-exporting C-header ceremony into runtime callsites.

#### Scenario: Internal runtime include boundary
- **GIVEN** runtime backend migration scope is `src/render/backend/`
- **WHEN** wave-1 migration is complete
- **THEN** runtime backend paths under `src/render/backend/` SHALL include the C++ wrapper header for filter-chain integration
- **AND** direct inclusion of `goggles_filter_chain.h` SHALL be absent from `src/render/backend/` for both quote and angle-bracket forms
- **AND** include-boundary verification command output SHALL show zero matches in that path

### Requirement: Stage Policy Preserves Pipeline Order Invariants

Wrapper stage policy controls SHALL only enable or disable known stages and SHALL preserve invariant stage order `pre-chain -> effect chain -> output pass`.

#### Scenario: Stage policy update
- **GIVEN** wrapper stage policy APIs receive a supported stage-mask configuration
- **WHEN** a caller applies stage policy through the wrapper
- **THEN** the policy SHALL affect enablement only for supported stage bits
- **AND** execution order SHALL remain `pre-chain -> effect chain -> output pass`
- **AND** regression tests SHALL assert this ordering under both all-enabled and subset-enabled stage masks

### Requirement: C ABI Continuity During Migration

Wave-1 migration SHALL preserve C ABI behavior and validation coverage.

#### Scenario: C ABI contract test continuity
- **GIVEN** C ABI contract tests are already present for `goggles_filter_chain.h`
- **WHEN** wave-1 wrapper migration is integrated
- **THEN** C ABI contract tests SHALL continue to compile and pass using `goggles_filter_chain.h`
- **AND** ABI boundary implementation files SHALL remain C-header based

### Requirement: Non-Regressive Blocker Handling

If migration encounters hidden blocker callsites, remediation SHALL preserve the C++ boundary in runtime code.

#### Scenario: Unexpected blocker callsite
- **GIVEN** a hidden runtime callsite blocks immediate wrapper adoption in wave-1 migration
- **WHEN** a blocker prevents immediate direct wrapper adoption in runtime flow
- **THEN** migration SHALL use an adapter/shim approach that keeps runtime callsites on C++ wrapper boundaries
- **AND** it SHALL NOT reintroduce direct runtime dependence on `goggles_filter_chain.h`
- **AND** implementation evidence SHALL record blocker path, shim/adaptor entrypoint, and follow-up removal task identifier

### Requirement: Installed Wrapper Consumer Contract

The filter-chain C++ wrapper SHALL remain consumable from the installed standalone package using
only the installed C ABI, boundary-owned wrapper/support headers, and required third-party headers.
Wrapper consumers SHALL NOT need Goggles-private `src/util/*` headers, Goggles backend types, or
Goggles source-tree-relative include roots.

#### Scenario: External C++ consumer includes installed wrapper
- **GIVEN** an external C++ consumer resolves the installed standalone filter-chain package
- **WHEN** it includes the installed wrapper header and compiles typed wrapper calls
- **THEN** the wrapper surface SHALL be complete using the installed package headers and required third-party dependencies
- **AND** the consumer SHALL NOT need Goggles-private support headers or Goggles backend implementation types

#### Scenario: Installed wrapper validation uses standalone-owned support
- **GIVEN** wrapper contract verification is run against the installed standalone package
- **WHEN** wrapper tests build and execute outside the Goggles repository
- **THEN** required wrapper-support contracts SHALL come from the standalone project itself
- **AND** verification SHALL NOT assume Goggles checkout-relative include layout

### Requirement: Wrapper Retarget Contract Survives Standalone Packaging

The C++ wrapper SHALL preserve the existing post-retarget public contract when consumed from the
installed standalone package. Successful format-only retarget SHALL preserve active preset identity
and source-independent runtime state, while explicit preset load or reload remains the rebuild path.

#### Scenario: Installed wrapper retarget preserves runtime state
- **GIVEN** backend or downstream C++ code holds a wrapper-owned runtime from the installed package
- **WHEN** it requests output-target retargeting for a format-only change
- **THEN** the wrapper contract SHALL preserve active preset identity and existing control state on success
- **AND** the callsite SHALL NOT need to express the change as preset reload

#### Scenario: Installed wrapper explicit reload remains rebuild behavior
- **GIVEN** downstream C++ code uses the installed wrapper with an active preset
- **WHEN** it explicitly loads or reloads a preset
- **THEN** the wrapper SHALL treat that request as full preset/runtime rebuild behavior
- **AND** the installed wrapper contract SHALL keep that path distinct from output-format retargeting

