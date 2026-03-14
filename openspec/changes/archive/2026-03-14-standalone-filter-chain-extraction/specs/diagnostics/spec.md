# Delta for diagnostics

## ADDED Requirements

### Requirement: Diagnostics Implementation Builds Without goggles_util

The diagnostics implementation (event model, sinks, session, ledger, policy, and related types)
SHALL build as part of the standalone filter-chain project without linking or depending on the
`goggles_util` CMake target. The diagnostics OBJECT library SHALL NOT transitively pull in
`toml11`, `BS_thread_pool`, or `Threads` as link dependencies.

#### Scenario: Standalone diagnostics target has no goggles_util dependency

- **GIVEN** the standalone project's diagnostics OBJECT library target
- **WHEN** its link dependencies are inspected (e.g., via `cmake --graphviz` or target property query)
- **THEN** `goggles_util` SHALL NOT appear as a direct or transitive link dependency
- **AND** `toml11`, `BS_thread_pool`, and `Threads` SHALL NOT appear as transitive link dependencies

#### Scenario: Diagnostics compiles outside Goggles source tree

- **GIVEN** the diagnostics implementation files have been moved to `filter-chain/src/diagnostics/`
- **WHEN** the standalone project is configured and built from a clean checkout
- **THEN** all diagnostics source files SHALL compile without errors
- **AND** compilation SHALL NOT require Goggles `src/util/` headers on the include path

#### Scenario: Diagnostics headers are self-contained within standalone tree

- **GIVEN** the ~10 diagnostics headers referenced by chain and shader code
- **WHEN** their transitive include dependencies are audited within the standalone tree
- **THEN** each header SHALL resolve all includes through standalone-owned paths
- **AND** no header SHALL include Goggles `util/config.hpp`, `util/job_system.hpp`, or other
  `goggles_util`-owned types

### Requirement: TOML-Based Policy Configuration Is Host-Side

TOML-based diagnostic policy configuration (reading `[diagnostics]` sections from `goggles.toml`)
SHALL remain a host-side concern owned by the Goggles application. The standalone diagnostics
library SHALL accept policy values through programmatic API parameters, not by reading TOML
configuration files directly.

#### Scenario: Standalone library accepts policy through API, not TOML

- **GIVEN** a diagnostic session is created through the standalone library's boundary API
- **WHEN** the caller specifies reporting mode, strict/compatibility policy, and tier
- **THEN** the library SHALL accept those values as API parameters
- **AND** the library SHALL NOT read TOML configuration files or depend on `toml11` for policy resolution

#### Scenario: Goggles host maps TOML config to API parameters

- **GIVEN** the Goggles application reads `[diagnostics]` from its TOML configuration
- **WHEN** it creates a diagnostic session on the filter-chain runtime
- **THEN** the host SHALL translate TOML configuration values into boundary API parameters
- **AND** the TOML parsing dependency SHALL remain in the Goggles host, not in the standalone library

### Requirement: Diagnostic Test Ownership Transfer

The standalone project SHALL own and run all diagnostics unit tests that verify diagnostic event
model, sink behavior, session lifecycle, ledger operations, and policy enforcement. Goggles SHALL
NOT retain copies of these tests.

#### Scenario: Diagnostics unit tests run from standalone project

- **GIVEN** diagnostics unit tests have been moved to `filter-chain/tests/`
- **WHEN** `ctest --test-dir filter-chain/build` is executed
- **THEN** all diagnostics unit tests SHALL pass using library-owned diagnostics test corpus assets
- **AND** test assertions SHALL verify event model, sink delivery, session identity, and ledger behavior

#### Scenario: Goggles does not retain diagnostics unit tests

- **GIVEN** the diagnostics unit test files have been moved to the standalone project
- **WHEN** the Goggles `tests/` directory is inspected
- **THEN** the moved diagnostics unit test files SHALL NOT be present
- **AND** Goggles MAY retain host integration tests that exercise diagnostic session creation
  through the filter-chain boundary API

## MODIFIED Requirements

### Requirement: Diagnostic Policy Configuration

_(Modification to scope of existing requirement)_

The diagnostics policy configuration requirement SHALL distinguish between the policy data model
(owned by the standalone library) and the TOML configuration surface (owned by the Goggles host).
The standalone library SHALL define the policy struct shape (`mode`, `capture_mode`, `tier`,
`capture_frame_limit`, `retention_bytes`, `promote_fallback_to_error`, `reflection_loss_is_fatal`)
and enforce policy semantics. The Goggles host SHALL own the mapping from TOML configuration keys
to that policy struct.
(Previously: The existing requirement described policy configuration including TOML `[diagnostics]`
section reading as a single unified concern without distinguishing library vs. host ownership.)

#### Scenario: Policy struct shape is library-owned

- **GIVEN** the standalone filter-chain library's diagnostics policy type
- **WHEN** a diagnostic session is created
- **THEN** the policy struct fields (`mode`, `capture_mode`, `tier`, `capture_frame_limit`,
  `retention_bytes`, `promote_fallback_to_error`, `reflection_loss_is_fatal`) SHALL be defined
  by the standalone library
- **AND** the library SHALL enforce policy semantics (strict vs. compatibility, tier activation)
  based on the provided struct values

#### Scenario: TOML mapping responsibility stays with host

- **GIVEN** Goggles configuration contains a `[diagnostics]` TOML section
- **WHEN** the application initializes diagnostic policy
- **THEN** the host application code SHALL parse TOML keys (`mode`, `strict`, `tier`,
  `capture_frame_limit`, `retention_bytes`) and construct the library-owned policy struct
- **AND** the standalone library SHALL NOT import or link TOML parsing libraries

## REMOVED Requirements

_(No requirements are removed by this change.)_
