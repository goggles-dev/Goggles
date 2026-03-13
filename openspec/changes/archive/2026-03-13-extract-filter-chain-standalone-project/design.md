# Design: Extract Filter Chain Standalone Project

> **Scope of current work:** This design describes the full extraction target. The current change
> implements Phase 1–2 monorepo groundwork (see `tasks.md`). Phases 3–4 of this design are future
> work that will be carried out in the standalone repository once created.

## Technical Approach

Goggles already treats the filter chain as a boundary-owned dependency with a preserved post-retarget
contract. This change does not rework that boundary. It extracts the existing library-owned slice
into a standalone CMake project that owns its own repository layout, support code, assets, package
metadata, and verification.

The extracted project keeps the same functional split:

- Goggles remains responsible for swapchain lifecycle, import, synchronization, submission, and
  presentation.
- The standalone library owns chain, shader, texture, diagnostics, asset resolution, and public C/C++
  boundary behavior.
- Goggles consumes the extracted library through the installed package surface.

Target standalone repository layout:

```text
filter-chain/
|- CMakeLists.txt
|- cmake/
|- include/
|- src/
|- tests/
`- assets/
```

Within that layout:

- `include/` contains the installed C header, C++ wrapper header, and installed public support
  headers.
- `src/` contains chain, shader, texture, diagnostics, and private support implementation.
- `tests/` contains installed-surface contract tests and downstream consumer validation.
- `assets/` contains the packaged shaders, presets, fixtures, and related data required by runtime
  behavior and contract verification.
- `cmake/` contains dependency discovery helpers and package config/export templates.

## Architecture Decisions

### Decision: Start from the current consumer boundary and extract only the standalone project concerns

**Choice**: Treat the public boundary, post-retarget behavior, and Goggles backend/controller
consumption model as fixed baseline.

**Alternatives considered**: Re-open the Goggles consumer boundary during extraction.

**Rationale**: Re-scoping those concerns here would blur the extraction goal and duplicate boundary
cleanup that is already in place.

### Decision: Extract the current reusable runtime slice into a standalone project root

**Choice**: Move the reusable implementation currently centered in `src/render/chain/`,
`src/render/shader/`, and `src/render/texture/` into a standalone project rooted at `include/`,
`src/`, `tests/`, `assets/`, and `cmake/`.

**Alternatives considered**: Keep publishing from Goggles-owned build fragments; keep a nested
subtree such as `standalone/` as the long-term repository layout.

**Rationale**: The change goal is an independent project, not a better in-repo slice. The extracted
repository root must already reflect the final owned layout.

### Decision: Move required support and diagnostics code under library ownership

**Choice**: Any support contract required by installed public headers or extracted implementation is
owned by the standalone project under `include/` or `src/`.

**Alternatives considered**: Keep depending on Goggles `src/util/*`; split support into a separate
shared utility package.

**Rationale**: Standalone extraction is incomplete until the library can build and verify without
Goggles-private support ownership.

### Decision: Package and verify the library through installed public surfaces

**Choice**: The standalone project installs headers, libraries, assets, and CMake package metadata,
then validates the installed package with contract tests and downstream consumer builds.

**Alternatives considered**: Accept source-tree tests as sufficient.

**Rationale**: Extraction success depends on the installed package contract, not on in-repo source
access.

### Decision: Ship static and shared variants only

**Choice**: The standalone package publishes supported `STATIC` and `SHARED` variants and does not
define a supported `MODULE` surface.

**Alternatives considered**: Single-variant export; `MODULE` plugin packaging.

**Rationale**: The supported linkage surface is explicit and downstream verification must cover both
forms.

### Decision: Goggles integrates as an external package first

**Choice**: Goggles resolves the library through `find_package(...)` as the normal dependency path.
`add_subdirectory(...)` remains optional only for explicit side-by-side development.

**Alternatives considered**: Keep source-based Goggles integration as the default acceptance path.

**Rationale**: The extracted library is only complete when Goggles consumes it like any other
downstream package consumer.

### Decision: Monorepo shared-variant presets are transitional only

**Choice**: The root `.shared` hidden preset and `test-shared` presets in Goggles `CMakePresets.json`
exist only as transitional host-integration coverage. They verify that Goggles builds and tests when
the in-repo `goggles-filter-chain` target is built as `SHARED`. They are not package/distribution
validation and must not be treated as such.

**Alternatives considered**: Remove them immediately during Phase 1 cleanup; keep them permanently as
a Goggles-owned shared-linkage check.

**Rationale**: Removing them now would leave no way to catch shared-build regressions during the
monorepo groundwork phases. Keeping them permanently would duplicate the standalone project's own
static/shared package validation and blur the ownership boundary. The correct lifecycle is:
1. Retain during Phases 1–4 as a lightweight host-integration guard.
2. Remove in Phase 5 once the standalone project owns real static/shared package validation.
3. If Goggles still needs a shared-linkage host check after extraction, it should consume the
   installed shared package through `find_package(...)`, not rebuild the library in-tree.

Do not add additional root shared-variant presets beyond the current `test-shared`.

## Data Flow

### Standalone build and install flow

```text
project root
  |- include/
  |- src/
  |- tests/
  |- assets/
  `- cmake/

cmake configure
  -> build static library
  -> build shared library
  -> install headers + libraries + assets + package metadata
  -> run installed-surface contract tests
  -> run downstream consumer package validation
```

### Runtime ownership after extraction

```text
Goggles backend
  -> FilterChainController
      -> installed C++ wrapper
          -> installed C ABI
              -> chain + shader + texture + diagnostics runtime
```

### Asset resolution after extraction

```text
installed consumer or installed contract test
  -> library asset lookup contract
  -> packaged asset root
  -> presets / shaders / fixtures opened from library-owned assets
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `CMakeLists.txt` | Create | Define the standalone top-level project, targets, install rules, export rules, and test enablement. |
| `cmake/FilterChainDependencies.cmake` | Create | Resolve third-party dependencies without Goggles-specific wrapper assumptions. |
| `cmake/GogglesFilterChainConfig.cmake.in` | Create | Provide package config metadata for installed consumers. |
| `include/goggles_filter_chain.h` | Create | Install the public C ABI header at the standalone project root. |
| `include/goggles_filter_chain.hpp` | Create | Install the public C++ wrapper header at the standalone project root. |
| `include/goggles/filter_chain/*.hpp` | Create | Install public support and diagnostics-facing headers owned by the standalone project. |
| `src/chain/*` | Create | Host chain runtime, parser, pass execution, frame history, and ABI adapter implementation. |
| `src/shader/*` | Create | Host shader runtime, preprocessing, reflection, and related implementation. |
| `src/texture/*` | Create | Host texture-loading implementation. |
| `src/diagnostics/*` | Create | Host library-owned diagnostics runtime support required by the contract. |
| `src/support/*` | Create | Host private support modules required by extracted implementation. |
| `assets/*` | Create | Host packaged shaders, presets, fixtures, and related runtime or verification data. |
| `tests/contract/*` | Create | Verify the installed public surface and preserved retarget contract. |
| `tests/consumer/*` | Create | Verify downstream `find_package(...)` consumption for static and shared variants. |
| Goggles CMake integration | Modify | Make installed package consumption the primary Goggles dependency path. |
| `src/render/CMakeLists.txt` | Modify | Stop owning the library package surface inside Goggles and link the external package target. |
| `tests/CMakeLists.txt` | Modify | Keep Goggles host verification only after reusable contract acceptance moves to the standalone project. |

## Interfaces / Contracts

### Public surface retained from the current baseline

The extracted project keeps the same public contract already established in the current tree:

- `goggles_filter_chain.h`
- `goggles_filter_chain.hpp`
- `goggles/filter_chain/error.hpp`
- `goggles/filter_chain/result.hpp`
- `goggles/filter_chain/filter_controls.hpp`
- `goggles/filter_chain/scale_mode.hpp`
- `goggles/filter_chain/vulkan_context.hpp`

Diagnostics public headers (`goggles/filter_chain/diagnostics/*.hpp`) are planned for Phase 4 and do
not exist in the current monorepo tree. Diagnostics types are currently defined only in the C API
header and C++ wrapper.

The change is about ownership and packaging of that contract, not contract redesign.

### Support ownership rules

1. Installed public headers MUST not depend on Goggles-private headers.
2. Extracted implementation MUST not require Goggles-private support ownership.
3. Support types needed by the public contract belong under installed library ownership.
4. Support types needed only by implementation belong under uninstalled private library ownership.

During the monorepo groundwork phase, public headers under `goggles/filter_chain/` define support
types inline using shared `#ifndef` guards that match the monorepo `util/` counterparts. This
prevents ODR violations while both copies coexist. The shared-guard approach is transitional and
will be eliminated when the standalone project owns the sole copy of each type.

### Export model

Installed consumer contract:

```cmake
find_package(GogglesFilterChain CONFIG REQUIRED)

target_link_libraries(consumer PRIVATE GogglesFilterChain::goggles-filter-chain)
```

Export rules:

- `GogglesFilterChain::goggles-filter-chain` is always available.
- `GogglesFilterChain::goggles-filter-chain-static` and
  `GogglesFilterChain::goggles-filter-chain-shared` are exported for explicit validation.
- Package metadata uses standard dependency discovery and imported targets.
- Package metadata does not require Goggles source paths, Pixi wrappers, or Conda-specific
  environment variables.

### Goggles integration contract after extraction

- Installed package consumption is the normal and release-relevant path.
- Local source wiring may remain available for explicit side-by-side development.
- Goggles backend code stays on the installed public boundary and does not regain direct access to
  chain internals.

## Testing Strategy

| Layer | What to Test | Approach |
|-------|--------------|----------|
| Unit | Parser, preprocessor, reflection, control helpers, support utilities | Run library-owned tests from the standalone project. |
| Contract | C ABI, C++ wrapper, post-retarget behavior, diagnostics summaries | Compile and run against installed headers, installed libraries, and installed assets. |
| Consumer | Package usability | Build out-of-tree `find_package(...)` consumers for static and shared variants. |
| Asset | Packaged shader, preset, and fixture lookup | Verify runtime behavior without cwd or Goggles-repository assumptions. |
| Goggles integration | Host-owned behavior with external package consumption | Keep Goggles tests focused on controller/backend ownership and package integration. |

## Migration / Rollout

> **Relationship to `tasks.md` phases:** The phases below describe the standalone extraction
> lifecycle. `tasks.md` Phase 1–2 (monorepo groundwork) prepares the boundary for these phases.
> `tasks.md` Phase 3+ maps to the phases below.

### Phase 1: Create the standalone project root

- Create the project-root `CMakeLists.txt`, `cmake/`, `include/`, `src/`, `tests/`, and `assets/`
  layout.
- Move the reusable implementation and public headers into that owned structure.

### Phase 2: Move support and assets under library ownership

- Replace remaining Goggles-private support dependencies with library-owned support modules.
- Move diagnostics support, fixtures, presets, and shader assets under library ownership.

### Phase 3: Build, install, and export the package

- Publish static and shared libraries.
- Install headers, package metadata, and assets together.
- Export package targets for downstream discovery.

### Phase 4: Verify installed surfaces and switch Goggles to package-first consumption

- Run installed-surface contract tests and downstream consumer validation.
- Update Goggles to consume the installed package through `find_package(...)` as the normal path.
- Keep Goggles verification focused on host-owned behavior and preserved post-retarget semantics.

## Open Questions

- [ ] None.
