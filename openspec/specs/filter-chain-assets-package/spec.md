# filter-chain-assets-package Specification

## Purpose

Define the standalone filter-chain asset package contract used by installed verification and
downstream consumers.

## Requirements

### Requirement: Library-Owned Asset Package

The standalone filter-chain project SHALL publish a library-owned asset package that contains the
fixtures, presets, shader assets, and related data needed for installed contract verification and
documented downstream consumption. Those assets SHALL be owned and versioned by the standalone
project rather than by the Goggles repository.

#### Scenario: Installed project exposes standalone-owned assets
- **GIVEN** the standalone filter-chain project has been installed or staged for distribution
- **WHEN** maintainers inspect the installed asset content used for contract verification
- **THEN** required presets, shaders, and related fixtures SHALL be present as standalone project-owned assets
- **AND** those verification assets SHALL NOT be sourced from Goggles-owned directories

### Requirement: Asset Resolution Is Package-Oriented

Consumers and installed verification flows SHALL resolve standalone filter-chain assets through the
standalone project's documented package-oriented asset location rules. Asset lookup SHALL NOT depend
on Goggles checkout-relative paths or the caller's current working directory.

#### Scenario: Installed tests resolve assets without repository context
- **GIVEN** installed contract tests or sample consumers run outside the standalone project source tree
- **WHEN** they load packaged presets or shader assets
- **THEN** asset resolution SHALL succeed through the standalone package's documented asset location contract
- **AND** success SHALL NOT depend on current working directory or Goggles repository-relative paths

### Requirement: Assets Support Public-Surface Validation

The standalone asset package SHALL provide the minimum reusable content needed to verify the public
surface from installed `STATIC` and `SHARED` distributions.

#### Scenario: Public-surface validation reuses the same owned assets
- **GIVEN** maintainers validate both installed `STATIC` and installed `SHARED` package consumption paths
- **WHEN** contract verification is executed against each distribution form
- **THEN** both validation flows SHALL use the standalone library-owned asset package
- **AND** neither validation flow SHALL require a separate Goggles-owned fixture source

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
