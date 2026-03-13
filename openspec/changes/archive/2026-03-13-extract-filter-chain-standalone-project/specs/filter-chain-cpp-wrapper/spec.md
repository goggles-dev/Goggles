# Delta for filter-chain-cpp-wrapper

## ADDED Requirements

### Requirement: Installed Wrapper Consumer Contract

The filter-chain C++ wrapper SHALL remain consumable from the installed standalone package using
only the installed C ABI, boundary-owned wrapper/support headers, and required third-party headers.
Wrapper consumers SHALL NOT need Goggles-private `src/util/*` headers, Goggles backend types, or
Goggles source-tree-relative include roots.

#### Scenario: External C++ consumer includes installed wrapper
- GIVEN an external C++ consumer resolves the installed standalone filter-chain package
- WHEN it includes the installed wrapper header and compiles typed wrapper calls
- THEN the wrapper surface SHALL be complete using the installed package headers and required third-party dependencies
- AND the consumer SHALL NOT need Goggles-private support headers or Goggles backend implementation types

#### Scenario: Installed wrapper validation uses standalone-owned support
- GIVEN wrapper contract verification is run against the installed standalone package
- WHEN wrapper tests build and execute outside the Goggles repository
- THEN required wrapper-support contracts SHALL come from the standalone project itself
- AND verification SHALL NOT assume Goggles checkout-relative include layout

### Requirement: Wrapper Retarget Contract Survives Standalone Packaging

The C++ wrapper SHALL preserve the existing post-retarget public contract when consumed from the
installed standalone package. Successful format-only retarget SHALL preserve active preset identity
and source-independent runtime state, while explicit preset load or reload remains the rebuild path.

#### Scenario: Installed wrapper retarget preserves runtime state
- GIVEN backend or downstream C++ code holds a wrapper-owned runtime from the installed package
- WHEN it requests output-target retargeting for a format-only change
- THEN the wrapper contract SHALL preserve active preset identity and existing control state on success
- AND the callsite SHALL NOT need to express the change as preset reload

#### Scenario: Installed wrapper explicit reload remains rebuild behavior
- GIVEN downstream C++ code uses the installed wrapper with an active preset
- WHEN it explicitly loads or reloads a preset
- THEN the wrapper SHALL treat that request as full preset/runtime rebuild behavior
- AND the installed wrapper contract SHALL keep that path distinct from output-format retargeting
