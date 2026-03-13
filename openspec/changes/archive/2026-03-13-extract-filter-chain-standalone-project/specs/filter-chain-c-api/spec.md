# Delta for filter-chain-c-api

## ADDED Requirements

### Requirement: Installed C ABI Consumer Contract

The filter-chain C API public surface MUST remain consumable from the installed standalone package
without Goggles repository context. The installed C header and exported package metadata MUST be
sufficient for external C consumers to compile, link, and use the ABI without Goggles-private
headers, source-tree-relative include roots, or Goggles-owned fixtures.

#### Scenario: External C consumer uses installed header only
- GIVEN an external C consumer points include and link settings at the installed standalone package
- WHEN it compiles against the installed filter-chain C header
- THEN the consumer SHALL obtain all required public declarations from the installed package surface
- AND compilation SHALL NOT require Goggles source-tree headers or Goggles-private `src/util/*` dependencies

#### Scenario: Installed ABI validation uses library-owned fixtures
- GIVEN C ABI contract verification runs against the installed standalone package
- WHEN those checks exercise preset loading, controls, or retarget behavior
- THEN required fixtures and assets SHALL come from the standalone library-owned asset package
- AND the verification SHALL NOT depend on Goggles-owned test data paths

### Requirement: Post-Retarget Output Contract Persists Outside Goggles Tree

The installed standalone C ABI MUST preserve the existing post-retarget public contract. A
successful format-only retarget for an unchanged preset MUST preserve active preset identity,
control state, and other source-independent preset-derived runtime state, while explicit preset load
or reload remains the full rebuild path.

#### Scenario: Installed consumer retarget preserves preset-derived state
- GIVEN an external consumer uses the installed C ABI with a runtime in READY state
- WHEN it requests output-target retargeting for a format-only change
- THEN the runtime SHALL preserve active preset identity and existing control state on success
- AND the consumer SHALL NOT need to reload the preset merely to adopt the new output target

#### Scenario: Installed consumer explicit reload remains distinct
- GIVEN an external consumer uses the installed C ABI with an active preset
- WHEN it explicitly loads or reloads a preset
- THEN that request SHALL remain the full preset/runtime rebuild path
- AND the installed ABI contract SHALL keep that path distinct from format-only retarget behavior
