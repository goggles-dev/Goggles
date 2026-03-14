# Delta for goggles-filter-chain

## ADDED Requirements

### Requirement: In-Repo Subdirectory Bridge During Extraction

The standalone filter-chain project SHALL initially reside as an in-repo subdirectory
(`filter-chain/`) within the Goggles repository. During the transitional extraction period,
Goggles SHALL consume the standalone target through `add_subdirectory(filter-chain/)` before
switching to `find_package()` consumption. The bridge period SHALL NOT introduce include-path
coupling back to the Goggles source tree.

#### Scenario: Transitional bridge keeps Goggles building

- **GIVEN** the standalone project skeleton exists at `filter-chain/` within the Goggles repository
- **WHEN** Goggles is configured with `add_subdirectory(filter-chain/)` as the integration path
- **THEN** the Goggles build SHALL succeed with all existing tests passing
- **AND** the standalone target SHALL expose only installed-surface include paths to the Goggles consumer

#### Scenario: Bridge does not re-introduce include-path coupling

- **GIVEN** Goggles consumes the standalone target through the `add_subdirectory()` bridge
- **WHEN** the standalone target's include directories are inspected
- **THEN** no include directory SHALL reference `${CMAKE_SOURCE_DIR}/src` or `${CMAKE_SOURCE_DIR}/src/render`
- **AND** Goggles code that compiles against the standalone target SHALL NOT resolve includes through Goggles source-tree layout assumptions

#### Scenario: Bridge is removed after package-first switch

- **GIVEN** Goggles has been switched to `find_package(GogglesFilterChain)` as the primary path
- **WHEN** the `add_subdirectory(filter-chain/)` bridge is inspected
- **THEN** the bridge MAY remain as an optional local-development convenience
- **AND** the bridge SHALL NOT be required for Goggles release acceptance

### Requirement: Standalone Source Tree Include-Path Isolation

All source files under the standalone project tree SHALL use standalone-relative include paths
exclusively. The standalone source tree SHALL NOT contain any include directives that assume
the Goggles source-tree layout.

#### Scenario: No Goggles util includes in standalone tree

- **GIVEN** all source and header files under `filter-chain/src/`
- **WHEN** include directives are audited (e.g., `grep -r '#include <util/' filter-chain/src/`)
- **THEN** zero matches SHALL be found for `#include <util/...>` paths
- **AND** zero matches SHALL be found for `#include <render/...>` paths that reference Goggles-layout modules

#### Scenario: Standalone internal includes use project-relative paths

- **GIVEN** implementation files under `filter-chain/src/`
- **WHEN** they include library-internal headers
- **THEN** include paths SHALL resolve through the standalone project's own include directories
- **AND** include paths SHALL NOT depend on Goggles `${CMAKE_SOURCE_DIR}/src` being on the include path

### Requirement: Library-Owned Support Shim Contracts

The standalone project SHALL provide library-owned support shims for logging, profiling, and
serialization that replace the Goggles `util/logging.hpp`, `util/profiling.hpp`, and
`util/serializer.hpp` dependencies. Each shim SHALL satisfy the same interface contract as the
Goggles original while depending only on the shim's own third-party dependency (spdlog for logging,
Tracy for profiling, standard library for serializer).

#### Scenario: Logging shim preserves tag-based facade contract

- **GIVEN** the standalone project's library-owned logging shim
- **WHEN** chain, shader, and texture implementation files use the logging facade
- **THEN** the shim SHALL support the same tagged logging macros (e.g., `GOGGLES_LOG_INFO`, `GOGGLES_LOG_WARN`)
- **AND** log tags (`render.chain`, `render.shader`, `render.texture`) SHALL be preserved
- **AND** the shim SHALL depend only on spdlog, not on Goggles `util/logging.hpp`

#### Scenario: Profiling shim compiles without Tracy when Tracy is unavailable

- **GIVEN** the standalone project is built without Tracy available in the build environment
- **WHEN** profiling macros are expanded in implementation files
- **THEN** profiling macros SHALL expand to no-op expressions
- **AND** compilation SHALL succeed without Tracy headers or libraries

#### Scenario: Serializer shim is self-contained

- **GIVEN** the standalone project's serializer shim under `filter-chain/src/support/`
- **WHEN** `shader_runtime.cpp` uses serialization utilities
- **THEN** the shim SHALL provide the required serialization interface using only the standard library
- **AND** the shim SHALL NOT include Goggles `util/serializer.hpp`

## MODIFIED Requirements

_(No existing requirements are modified by this change. The main spec requirements for
Standalone Filter Library Target, Library-Owned Support Boundary, and Installed Public-Surface
Verification Boundary are being implemented as specified.)_

## REMOVED Requirements

_(No requirements are removed by this change.)_
