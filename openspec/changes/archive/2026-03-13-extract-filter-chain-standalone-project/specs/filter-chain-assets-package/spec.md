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
- GIVEN the standalone filter-chain project has been installed or staged for distribution
- WHEN maintainers inspect the installed asset content used for contract verification
- THEN required presets, shaders, and related fixtures SHALL be present as standalone project-owned assets
- AND those verification assets SHALL NOT be sourced from Goggles-owned directories

### Requirement: Asset Resolution Is Package-Oriented

Consumers and installed verification flows SHALL resolve standalone filter-chain assets through the
standalone project's documented package-oriented asset location rules. Asset lookup SHALL NOT depend
on Goggles checkout-relative paths or the caller's current working directory.

#### Scenario: Installed tests resolve assets without repository context
- GIVEN installed contract tests or sample consumers run outside the standalone project source tree
- WHEN they load packaged presets or shader assets
- THEN asset resolution SHALL succeed through the standalone package's documented asset location contract
- AND success SHALL NOT depend on current working directory or Goggles repository-relative paths

### Requirement: Assets Support Public-Surface Validation

The standalone asset package SHALL provide the minimum reusable content needed to verify the public
surface from installed `STATIC` and `SHARED` distributions.

#### Scenario: Public-surface validation reuses the same owned assets
- GIVEN maintainers validate both installed `STATIC` and installed `SHARED` package consumption paths
- WHEN contract verification is executed against each distribution form
- THEN both validation flows SHALL use the standalone library-owned asset package
- AND neither validation flow SHALL require a separate Goggles-owned fixture source
