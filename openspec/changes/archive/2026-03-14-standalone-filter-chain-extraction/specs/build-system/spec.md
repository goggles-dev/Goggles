# Delta for build-system

## ADDED Requirements

### Requirement: Transitional Preset Cleanup After Package Validation

The build system SHALL remove the transitional `.shared` and `test-shared` CMake presets from
`CMakePresets.json` once the standalone project's package export proves both STATIC and SHARED
consumption paths. These presets SHALL NOT remain in the preset file after successful package
validation.

#### Scenario: Transitional presets removed after both linkage modes proven

- **GIVEN** the standalone project has been installed and downstream consumer validation has
  proven both STATIC and SHARED linkage through `find_package(GogglesFilterChain)`
- **WHEN** maintainers inspect `CMakePresets.json`
- **THEN** the `.shared` and `test-shared` presets SHALL have been removed
- **AND** existing named presets (`debug`, `release`, `asan`, `quality`, `test`) SHALL remain unchanged

#### Scenario: Presets not removed prematurely

- **GIVEN** the standalone project has NOT yet completed consumer validation for both linkage modes
- **WHEN** maintainers inspect `CMakePresets.json`
- **THEN** the `.shared` and `test-shared` presets SHALL still be present
- **AND** those presets SHALL continue to function for transitional verification

### Requirement: Standalone Dependency Discovery Module

The standalone project SHALL provide a `cmake/FilterChainDependencies.cmake` module that
encapsulates third-party dependency discovery for all required external libraries. The module
SHALL use standard `find_package()` calls and SHALL NOT require Goggles-specific Find modules
or Pixi/Conda-specific path assumptions.

#### Scenario: Dependency module discovers all required third-party libraries

- **GIVEN** the standalone project is configured from a clean checkout with dependencies available
  through standard CMake package discovery
- **WHEN** `cmake/FilterChainDependencies.cmake` is included during configuration
- **THEN** the module SHALL resolve Vulkan, expected-lite, spdlog, slang, stb_image, and Catch2
  through standard `find_package()` calls
- **AND** configuration SHALL NOT require Goggles-owned CMake Find modules

#### Scenario: Dependency module is reusable by exported package config

- **GIVEN** the standalone project has been installed and a downstream consumer resolves the package
- **WHEN** the exported `GogglesFilterChainConfig.cmake` is loaded by the consumer
- **THEN** the package config SHALL invoke dependency discovery for transitive PUBLIC dependencies
  (Vulkan, expected-lite)
- **AND** PRIVATE dependencies (spdlog, slang, stb_image) SHALL NOT leak to the consumer

### Requirement: Downstream Consumer Validation Projects

The standalone project SHALL include out-of-tree consumer validation projects that prove the
installed package is discoverable and linkable for both STATIC and SHARED library outputs. These
validation projects SHALL use only `find_package(GogglesFilterChain CONFIG REQUIRED)` and the
installed public surface.

#### Scenario: Static consumer validation succeeds

- **GIVEN** the standalone project has been installed to a prefix with STATIC library output
- **WHEN** a consumer validation project configures with `CMAKE_PREFIX_PATH` pointing to the
  install prefix
- **THEN** `find_package(GogglesFilterChain CONFIG REQUIRED)` SHALL succeed
- **AND** the consumer SHALL compile and link against the STATIC target without errors

#### Scenario: Shared consumer validation succeeds

- **GIVEN** the standalone project has been installed to a prefix with SHARED library output
- **WHEN** a consumer validation project configures with `CMAKE_PREFIX_PATH` pointing to the
  install prefix
- **THEN** `find_package(GogglesFilterChain CONFIG REQUIRED)` SHALL succeed
- **AND** the consumer SHALL compile and link against the SHARED target without errors

#### Scenario: Consumer validation uses only installed surface

- **GIVEN** a consumer validation project
- **WHEN** its source files are inspected
- **THEN** it SHALL include only headers from the installed package include root
- **AND** it SHALL NOT reference standalone project source-tree paths or Goggles-owned headers

### Requirement: Host Test Split After Extraction

After contract tests are moved to the standalone project, the Goggles test suite SHALL retain
only host integration tests that exercise the wiring between Goggles backend subsystems and the
filter-chain boundary. The Goggles test target SHALL NOT duplicate contract tests that are
owned and run by the standalone project.

#### Scenario: Goggles retains host integration tests

- **GIVEN** the standalone project owns ~22 contract test files
- **WHEN** the Goggles `tests/CMakeLists.txt` is inspected
- **THEN** it SHALL include host integration tests (e.g., `test_filter_boundary_contracts.cpp`,
  `test_vulkan_backend_subsystem_contracts.cpp`, `test_filter_chain_retarget.cpp`)
- **AND** those tests SHALL compile against the installed or subdirectory-provided filter-chain target

#### Scenario: Moved contract tests are absent from Goggles

- **GIVEN** ~22 contract test files have been moved to `filter-chain/tests/`
- **WHEN** the Goggles `tests/` directory is inspected
- **THEN** the moved contract test source files SHALL NOT be present in the Goggles test directory
- **AND** the Goggles test CMakeLists SHALL NOT reference the moved source files

## MODIFIED Requirements

### Requirement: CMake-First Standalone Filter Project Workflow

_(Addendum to existing requirement)_

The standalone project's `cmake/GogglesFilterChainConfig.cmake.in` template SHALL generate a
package config file that exports `GogglesFilterChain::goggles-filter-chain` and explicit
static/shared component targets. The generated config SHALL invoke
`FilterChainDependencies.cmake` for transitive dependency resolution.
(Previously: The existing requirement specified CMake-first workflow and `find_package()` discovery
but did not specify the config template mechanism or dependency forwarding contract.)

#### Scenario: Package config template generates valid export

- **GIVEN** the standalone project is installed to a prefix
- **WHEN** a downstream consumer calls `find_package(GogglesFilterChain CONFIG REQUIRED)`
- **THEN** the generated config SHALL define `GogglesFilterChain::goggles-filter-chain` as an
  imported target
- **AND** the config SHALL resolve transitive PUBLIC dependencies before defining the target

## REMOVED Requirements

_(No requirements are removed by this change.)_
