# Delta for goggles-filter-chain

## MODIFIED Requirements

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

## ADDED Requirements

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
