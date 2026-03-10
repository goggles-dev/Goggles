## MODIFIED Requirements

### Requirement: Boundary Isolation
The C header SHALL remain the ABI boundary contract only. Internal runtime C++ modules in runtime
backend scope SHALL use the C++ wrapper header for filter-chain integration, SHALL NOT directly
include or call the C ABI header, and SHALL NOT depend on internal chain runtime/build/resource/
control types. Wrapper adoption SHALL avoid re-exporting C-header ceremony or internal runtime types
into runtime callsites.

#### Scenario: Internal runtime include boundary
- **GIVEN** runtime backend migration scope is `src/render/backend/`
- **WHEN** the gate-centered refactor is complete
- **THEN** runtime backend paths under `src/render/backend/` SHALL include the C++ wrapper header for
  filter-chain integration
- **AND** direct inclusion of `goggles_filter_chain.h` SHALL be absent from that path for both quote
  and angle-bracket forms
- **AND** direct dependence on internal filter-chain runtime/build/resource/control types SHALL be
  absent from that path
- **AND** include-boundary verification command output SHALL show zero matches for the forbidden
  boundary leaks

### Requirement: C ABI Continuity During Migration
Filter-chain runtime refactors SHALL preserve C ABI behavior and validation coverage while internal
runtime ownership changes behind the ABI handle.

#### Scenario: C ABI contract test continuity
- **GIVEN** C ABI contract tests are already present for `goggles_filter_chain.h`
- **WHEN** the gate-centered refactor is integrated
- **THEN** C ABI contract tests SHALL continue to compile and pass using `goggles_filter_chain.h`
- **AND** ABI boundary implementation files SHALL remain C-header based
- **AND** the internal runtime replacement SHALL NOT require source changes in stable C ABI consumers

## ADDED Requirements

### Requirement: Stable Gate Surface During Internal Runtime Refactor
The C++ wrapper and C ABI SHALL remain the only stable filter-chain gates while internal runtime
ownership changes behind them. Public wrapper and ABI consumers SHALL continue to use the existing
gate entrypoints without learning about internal `ChainRuntime`, builder, executor, resource, or
control types.

#### Scenario: Stable gate behavior survives internal replacement
- **GIVEN** a stable gate consumer uses `FilterChainRuntime` or `goggles_filter_chain.h`
- **WHEN** the internal runtime implementation is replaced or decomposed behind the gate
- **THEN** the consumer SHALL continue using the same stable gate entrypoints and ownership model
- **AND** internal runtime type names SHALL remain hidden from the stable gate surface
