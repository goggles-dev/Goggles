# Delta for build-system

## ADDED Requirements

### Requirement: Normalized Filter Library Source Selection

The build system SHALL provide `goggles-filter-chain` as a stable Goggles consumer target contract
independent of whether the implementation is resolved from in-tree sources, a local subdirectory
provider, or the monorepo package/install rehearsal path.

#### Scenario: Goggles provider modes share target identity
- GIVEN Goggles is configured once with `in-tree`, once with `subdir`, and once with `package`
  provider selection
- WHEN downstream render targets link the filter runtime
- THEN each configuration SHALL provide the target name `goggles-filter-chain`
- AND downstream Goggles CMake code SHALL NOT branch on provider-specific target names

#### Scenario: Provider module normalizes external target names
- GIVEN an external provider exposes a namespaced or differently named target
- WHEN Goggles resolves that provider
- THEN the provider module SHALL normalize it to a local target named `goggles-filter-chain`
- AND downstream Goggles targets SHALL continue linking the normalized target name

### Requirement: Monorepo Package Rehearsal Preserves Goggles Workflows

The build system SHALL support a monorepo rehearsal where Goggles installs and then consumes the
filter-chain boundary through `package` mode without requiring ad-hoc build directories or non-preset
workflows.

#### Scenario: Preset-driven rehearsal remains available
- GIVEN Pixi tasks and named CMake presets are the supported workflow surface
- WHEN maintainers rehearse the package consumer path
- THEN the rehearsal SHALL run through repository-managed presets and task wrappers
- AND the workflow SHALL remain compatible with Goggles developer and CI conventions

#### Scenario: Installed rehearsal stays scoped to Goggles consumption
- GIVEN the package rehearsal installs boundary headers and config files inside the monorepo build
  tree
- WHEN Goggles consumes that install tree
- THEN the rehearsal SHALL prove Goggles can consume the normalized target and header surface
- AND it SHALL NOT by itself imply external downstream readiness beyond Goggles

### Requirement: Filter Runtime Test Registration Split

The build and test workflow SHALL distinguish reusable filter-chain contract coverage from Goggles
host/backend coverage.

#### Scenario: Contract and host coverage stay separately identifiable
- GIVEN automated test targets are registered for filter-chain behavior
- WHEN maintainers inspect the test graph
- THEN reusable contract coverage SHALL remain separately identifiable from Goggles host/backend
  coverage
- AND the split SHALL preserve clear ownership of boundary behavior versus host behavior

#### Scenario: Package consumer keeps host coverage active
- GIVEN Goggles is configured in `package` provider mode for the monorepo rehearsal
- WHEN tests are registered in that consumer build
- THEN Goggles host/backend coverage SHALL remain executable against the normalized consumer boundary
- AND reusable contract coverage MAY remain attached to the in-tree authored boundary target
