# Delta for build-system

## ADDED Requirements

### Requirement: CMake-First Standalone Filter Project Workflow

The build system SHALL define the extracted filter runtime as a CMake-first standalone project with
repository layout rooted at `include/`, `src/`, `tests/`, `assets/`, and `cmake/`. The documented
configure, build, test, install, and export workflow SHALL be runnable from a clean checkout without
requiring Goggles-specific wrappers.

#### Scenario: Clean checkout uses standalone CMake entry points
- GIVEN a clean checkout of the extracted filter-chain project
- WHEN a maintainer follows the documented standalone workflow
- THEN configure, build, test, and install steps SHALL execute through project-owned CMake entry points
- AND the workflow SHALL NOT require Pixi task wrappers, Goggles preset files, or Conda-specific environment assumptions

#### Scenario: Separate consumer validates exported package
- GIVEN the standalone project has been installed to a prefix
- WHEN a separate CMake consumer project resolves that install tree
- THEN package discovery, target resolution, and public-header inclusion SHALL succeed through the exported package contract
- AND validation SHALL occur without adding the library sources back into the consumer source tree

### Requirement: Goggles External Dependency Primary Path

Goggles SHALL consume the extracted filter runtime through `find_package(...)` as the primary
integration path. `add_subdirectory(...)` MAY remain available only as an explicit local-development
convenience and SHALL NOT be the required or default downstream integration contract.

#### Scenario: Goggles normal integration uses package discovery
- GIVEN Goggles is configured against an installed standalone filter-chain package
- WHEN render targets resolve the filter runtime dependency
- THEN Goggles SHALL obtain the dependency through `find_package(...)`
- AND normal integration guidance SHALL treat package discovery as the primary supported path

#### Scenario: Local development subdirectory path stays optional
- GIVEN a developer is iterating on Goggles and a local checkout of the extracted library together
- WHEN the developer opts into a local-development source-based workflow
- THEN `add_subdirectory(...)` MAY be used to wire that checkout for development convenience
- AND Goggles release acceptance SHALL NOT depend on `add_subdirectory(...)` being the primary consumer path

### Requirement: Paired Static and Shared Package Outputs

The standalone build and export workflow SHALL publish supported `STATIC` and `SHARED` library
outputs for the filter runtime. The exported package contract SHALL validate both output forms and
SHALL NOT require a `MODULE` target.

#### Scenario: Package exports static and shared variants
- GIVEN the standalone project is built for distribution
- WHEN install and export artifacts are inspected
- THEN supported package artifacts SHALL include both `STATIC` and `SHARED` library outputs
- AND consumers SHALL not be required to build or load a `MODULE` target to use the package

#### Scenario: Downstream validation covers both supported output forms
- GIVEN downstream consumer validation is run against the installed standalone package
- WHEN maintainers verify supported linkage modes
- THEN the validation evidence SHALL cover consumption of both `STATIC` and `SHARED` outputs
- AND success criteria SHALL not treat a `MODULE` build as an acceptable substitute for either supported output form
