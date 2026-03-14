# Delta for filter-chain-assets-package

## ADDED Requirements

### Requirement: Asset Category Coverage

The standalone asset package SHALL contain three distinct asset categories that together provide
sufficient content for all library-owned contract and integration tests.

#### Scenario: Test shader fixtures are present

- **GIVEN** the standalone asset package under `filter-chain/assets/`
- **WHEN** test shader fixture content is inspected
- **THEN** the package SHALL include preset files (`.slangp`) and shader source files (`.slang`)
  used by contract tests for format handling, history, feedback, frame count, pragma parsing,
  and format decoding
- **AND** these fixtures SHALL NOT be symlinks to or copies resolved from `shaders/retroarch/test/`
  in the Goggles repository at build time

#### Scenario: Diagnostics test corpus is present

- **GIVEN** the standalone asset package under `filter-chain/assets/`
- **WHEN** diagnostics test corpus content is inspected
- **THEN** the package SHALL include the authoring corpus (valid, invalid, and reflection test
  cases) and semantic probe presets used by diagnostics unit tests
- **AND** these assets SHALL NOT be resolved from Goggles `tests/util/test_data/` paths at build time

#### Scenario: Curated upstream shader subset is present

- **GIVEN** the standalone asset package under `filter-chain/assets/`
- **WHEN** upstream shader content is inspected
- **THEN** the package SHALL include only the specific upstream shader files referenced by
  zfast integration tests and shader validation tests
- **AND** the package SHALL NOT mirror the full `shaders/retroarch/` upstream collection

### Requirement: Test Fixture Resolution Through Compile Definition

Contract tests in the standalone project SHALL resolve asset paths through a compile-time
definition rather than relying on working directory, source-tree-relative paths, or
Goggles-owned path macros.

#### Scenario: Tests use FILTER_CHAIN_ASSET_DIR for fixture lookup

- **GIVEN** contract tests under `filter-chain/tests/` that load presets or shader fixtures
- **WHEN** those tests resolve asset file paths
- **THEN** path construction SHALL use a `FILTER_CHAIN_ASSET_DIR` compile definition (or
  equivalent project-owned macro) pointing to the standalone asset root
- **AND** tests SHALL NOT use `GOGGLES_SOURCE_DIR` or any Goggles-checkout-relative path macro

#### Scenario: Asset resolution works from installed test prefix

- **GIVEN** the standalone project has been installed to a test prefix with assets
- **WHEN** installed contract tests resolve asset paths
- **THEN** the asset resolution mechanism SHALL locate installed assets through the package's
  documented asset location contract
- **AND** resolution SHALL NOT depend on the standalone project source tree being present

#### Scenario: Asset resolution works from build tree

- **GIVEN** the standalone project is built but not installed
- **WHEN** tests run from the build tree (e.g., via `ctest --test-dir filter-chain/build`)
- **THEN** the `FILTER_CHAIN_ASSET_DIR` definition SHALL point to the source-tree asset directory
- **AND** all contract tests SHALL resolve their fixtures without installation

## MODIFIED Requirements

_(No existing requirements are modified by this change.)_

## REMOVED Requirements

_(No requirements are removed by this change.)_
