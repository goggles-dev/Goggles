# Delta for goggles-filter-chain

## MODIFIED Requirements

### Requirement: Standalone Filter Library Target

The extracted filter runtime SHALL be an independently buildable, installable, and exportable
standalone CMake project that publishes the downstream target contract `goggles-filter-chain`.
The standalone project SHALL use repository layout rooted at `include/`, `src/`, `tests/`,
`assets/`, and `cmake/`. Release acceptance SHALL require `STATIC` and `SHARED` library outputs,
and SHALL NOT require or expose a `MODULE` library variant as part of the package contract.

#### Scenario: Standalone checkout builds without Goggles repository
- GIVEN a clean checkout of the extracted filter-chain project
- WHEN the documented CMake workflow configures and builds the project
- THEN the project SHALL build without requiring the Goggles source tree, Pixi wrappers, or Conda-specific paths
- AND the project layout consumed by that workflow SHALL be rooted at `include/`, `src/`, `tests/`, `assets/`, and `cmake/`

#### Scenario: Installed package preserves stable target identity
- GIVEN the standalone project has been installed and exported
- WHEN a downstream consumer resolves the package through CMake package discovery
- THEN the consumer SHALL obtain the filter runtime through the target contract `goggles-filter-chain`
- AND consuming the installed package SHALL NOT require downstream target renaming or source-tree include assumptions

#### Scenario: Distribution excludes module-only success criteria
- GIVEN the standalone project is prepared for release validation
- WHEN library artifacts and exported targets are inspected
- THEN `STATIC` and `SHARED` outputs SHALL both be available as supported deliverables
- AND no `MODULE` library variant SHALL be required for success or documented as part of the supported package surface

## ADDED Requirements

### Requirement: Library-Owned Support Boundary

The extracted library SHALL own every public contract and every library-private support contract
required to build and verify itself outside the Goggles repository. Public headers and library-owned
internal code SHALL NOT depend on Goggles-private `src/util/*` headers, Goggles application config
types, or Goggles source-tree include-layout assumptions.

#### Scenario: Public surface excludes Goggles-private support headers
- GIVEN an external consumer compiles against the standalone library public headers
- WHEN header dependencies are audited from the install include root
- THEN the public surface SHALL depend only on boundary-owned declarations and allowed third-party headers
- AND no public header SHALL require Goggles-private `src/util/*` includes or Goggles app-private types

#### Scenario: Library-owned internals build without Goggles-private support
- GIVEN the standalone project builds its library sources from its own `src/` tree
- WHEN library-owned internal sources are compiled outside the Goggles repository
- THEN required support code SHALL be provided by the standalone project itself
- AND compilation SHALL NOT depend on Goggles-private helper ownership or Goggles checkout-relative include paths

### Requirement: Installed Public-Surface Verification Boundary

Reusable contract verification for the extracted library SHALL run against the installed public
surface using library-owned fixtures and assets. Goggles host integration coverage MAY verify host
wiring, but SHALL NOT replace installed-surface proof for the standalone library contract.

#### Scenario: Installed contract tests stay boundary-only
- GIVEN the standalone library has been installed to a test prefix
- WHEN contract tests compile and run against the installed headers, libraries, and package metadata
- THEN those tests SHALL validate boundary behavior without including Goggles-private source headers
- AND passing Goggles in-tree tests alone SHALL NOT satisfy standalone contract verification

#### Scenario: Library-owned fixtures and assets back verification
- GIVEN reusable contract tests exercise presets, shaders, or related runtime assets
- WHEN those tests run against the installed public surface
- THEN required fixtures and assets SHALL come from the library-owned project content
- AND the tests SHALL NOT depend on Goggles-owned fixture directories or shader asset paths
