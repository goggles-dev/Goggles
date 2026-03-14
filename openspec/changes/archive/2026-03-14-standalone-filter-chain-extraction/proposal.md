# Proposal: Standalone Filter-Chain Extraction

## Intent

### Problem

The `goggles-filter-chain` library is architecturally independent -- it owns chain orchestration,
shader runtime, texture loading, diagnostics, and the public C/C++ boundary contract -- but it
cannot build, test, or verify outside the Goggles source tree. Every implementation file in chain/,
shader/, and texture/ includes `util/logging.hpp` and `util/profiling.hpp`. The diagnostics OBJECT
library links `goggles_util` which transitively pulls in `toml11`, `BS_thread_pool`, and `Threads`
-- dependencies the filter chain does not use. Internal `#include <render/chain/...>` and
`<util/...>` paths assume the Goggles `${CMAKE_SOURCE_DIR}/src` layout. Contract tests compile
against `goggles_render` and `goggles_util` rather than the installed public surface.

This coupling prevents:
- Independent release and versioning of the filter-chain library.
- External consumers using `find_package(GogglesFilterChain)` without the Goggles repo.
- Verification that the public surface is self-contained.
- Clear ownership boundaries between library-owned and host-owned concerns.

Phase 1-2 monorepo groundwork (archived) established the public header boundary, canonical include
paths, and the `GogglesFilterChain::goggles-filter-chain` ALIAS target. This change completes the
extraction by creating a provably standalone CMake project, severing remaining coupling, moving
ownership of assets and tests, and switching Goggles to package-first consumption.

### Why now

The public header surface is clean and self-contained. The OBJECT library composition model is
proven. The boundary contract specs (`goggles-filter-chain`, `filter-chain-c-api`,
`filter-chain-cpp-wrapper`, `filter-chain-assets-package`, `build-system`) already describe the
standalone target state. What remains is implementation: moving code, severing coupling, packaging,
and verifying.

## Scope

### In Scope

1. **Standalone project skeleton** (`filter-chain/`): Create an in-repo CMake subproject with the
   layout `CMakeLists.txt`, `cmake/`, `include/`, `src/`, `tests/`, `assets/` that configures and
   builds independently via `cmake -S filter-chain/`.

2. **Source migration**: Move the reusable implementation from `src/render/chain/`,
   `src/render/shader/`, `src/render/texture/`, and the diagnostics subset from
   `src/util/diagnostics/` into standalone-owned modules under `filter-chain/src/`.

3. **Support code ownership**: Create library-owned logging, profiling, and serializer shims under
   `filter-chain/src/support/` to replace the 13+ file dependency on `util/logging.hpp`,
   `util/profiling.hpp`, and `util/serializer.hpp`.

4. **Diagnostics decoupling**: Sever the `goggles_diagnostics` -> `goggles_util` link. Move the 4
   diagnostics `.cpp` files and their 10 referenced headers under standalone ownership. Eliminate
   the transitive dependency on `toml11`, `BS_thread_pool`, and `Threads`.

5. **Asset package**: Move test shader fixtures (`shaders/retroarch/test/`), diagnostics test corpus
   (`tests/util/test_data/filter_chain_diagnostics/`), and a curated subset of upstream shaders
   under `filter-chain/assets/`. Make runtime and test asset lookup package-oriented rather than
   Goggles-checkout-relative.

6. **Contract test migration**: Move ~22 reusable contract test files to `filter-chain/tests/`
   (C API contracts, retarget contract, filter controls, preset parser, preprocessor, reflect,
   shader runtime, semantic binder, zfast integration, all diagnostics unit tests). Keep 3 host
   integration tests in Goggles (`test_filter_boundary_contracts.cpp`,
   `test_vulkan_backend_subsystem_contracts.cpp`, `test_filter_chain_retarget.cpp`).

7. **Build, install, and export**: Publish `STATIC` and `SHARED` library targets. Install headers,
   libraries, assets, and CMake package metadata. Export
   `GogglesFilterChain::goggles-filter-chain` plus explicit static/shared targets via config-file
   packages. Create `cmake/FilterChainDependencies.cmake` for third-party discovery and
   `cmake/GogglesFilterChainConfig.cmake.in` for package metadata.

8. **Downstream consumer validation**: Add out-of-tree consumer validation tests that use
   `find_package(GogglesFilterChain CONFIG REQUIRED)` for both static and shared linkage.

9. **Goggles integration switch**: Update Goggles to consume the installed package through
   `find_package(...)` as the primary path, with `add_subdirectory(filter-chain/)` kept only as
   an explicit development convenience. Remove the transitional `.shared` and `test-shared` presets
   from `CMakePresets.json` once standalone package validation proves both linkage modes.

10. **End-to-end verification**: Prove the standalone project can configure, build, test, install,
    and validate from a clean checkout. Run Goggles build + host verification against the installed
    package.

### Out of Scope

- **Contract redesign**: The public C/C++ API surface, diagnostics event model, and boundary
  behavior are preserved as-is. No new APIs or behavioral changes.
- **Repository extraction**: The final `git subtree split` or move to a separate repository is
  deferred. This change produces a provably standalone subdirectory that is ready for extraction.
- **New Pixi environment for standalone project**: The standalone project uses standard
  `find_package()` for all dependencies. It does not get its own Pixi environment.
- **Upstream shader mirror changes**: The full `shaders/retroarch/` mirror stays in Goggles. The
  standalone project owns only a curated test subset under `filter-chain/assets/`.
- **CI pipeline for standalone project**: CI configuration for the extracted project is future work.
  This change validates via local build/test/install workflows.
- **MODULE library variant**: Not part of the supported package surface per existing specs.

## Approach

The extraction follows the phased strategy from the archived design, implemented as an in-repo
subdirectory (`filter-chain/`) per the key decision already made. Each phase produces a verifiable
intermediate state.

### Phase 3: Create standalone skeleton and move implementation

Create the `filter-chain/` root with a top-level `CMakeLists.txt` that defines the
`goggles-filter-chain` target from source files under `filter-chain/src/`. Move (not copy) the
chain, shader, and texture implementation files. Create library-owned support shims for logging,
profiling, and serialization that satisfy the same interface contracts as the `util/` originals but
depend only on spdlog (logging), Tracy (profiling), and standard library (serializer). Update all
internal include paths from `<render/chain/...>` and `<util/...>` to standalone-relative paths.
Move the ~22 contract test files to `filter-chain/tests/`.

The key ordering constraint: the standalone project must configure and build after this phase.
Goggles will temporarily use `add_subdirectory(filter-chain/)` to consume the target.

### Phase 4: Move assets and diagnostics under library ownership

Move diagnostics implementation (4 `.cpp` files, ~10 headers) under `filter-chain/src/diagnostics/`.
Sever the `goggles_util` link by replacing the 2 actual goggles_util dependencies in diagnostics
code (the `goggles_util` PUBLIC link exists because `goggles_diagnostics` lives under `src/util/`
and uses `${CMAKE_SOURCE_DIR}/src` includes -- not because it needs config, job_system, or toml11).
Create the library-owned asset package under `filter-chain/assets/` and update test fixtures to
resolve assets from the package rather than Goggles checkout paths.

### Phase 5: Build, install, export, and switch to package-first

Add CMake install rules, export sets, and package config templates. Publish both STATIC and SHARED
variants. Add downstream consumer validation projects. Switch Goggles `src/render/CMakeLists.txt`
from `add_subdirectory(filter-chain/)` to `find_package(GogglesFilterChain CONFIG REQUIRED)`.
Remove the transitional `.shared` and `test-shared` presets from `CMakePresets.json`.

### Phase 6: End-to-end verification

Prove clean-checkout standalone build. Run installed-surface contract tests. Run Goggles build +
host integration tests against the installed package. Verify preserved post-retarget behavior.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `filter-chain/` (new) | New | Standalone CMake project root with full layout |
| `filter-chain/CMakeLists.txt` | New | Top-level project definition, targets, install/export rules |
| `filter-chain/cmake/` | New | `FilterChainDependencies.cmake`, `GogglesFilterChainConfig.cmake.in` |
| `filter-chain/include/` | New | Installed public headers (moved from `src/render/chain/include/` and `api/`) |
| `filter-chain/src/chain/` | New | Chain runtime sources (moved from `src/render/chain/`) |
| `filter-chain/src/shader/` | New | Shader runtime sources (moved from `src/render/shader/`) |
| `filter-chain/src/texture/` | New | Texture loading sources (moved from `src/render/texture/`) |
| `filter-chain/src/diagnostics/` | New | Diagnostics implementation (moved from `src/util/diagnostics/`) |
| `filter-chain/src/support/` | New | Library-owned logging, profiling, serializer shims |
| `filter-chain/tests/` | New | ~22 contract test files (moved from `tests/render/`) |
| `filter-chain/assets/` | New | Test fixtures, curated shader subset, diagnostics test corpus |
| `src/render/CMakeLists.txt` | Modified | Remove in-tree chain/shader/texture targets; consume via `find_package()` |
| `src/render/chain/` | Removed | Sources moved to `filter-chain/src/chain/` |
| `src/render/shader/` | Removed | Sources moved to `filter-chain/src/shader/` |
| `src/render/texture/` | Removed | Sources moved to `filter-chain/src/texture/` |
| `src/util/diagnostics/` | Modified | Sources moved to `filter-chain/src/diagnostics/`; Goggles keeps the subdirectory only if host-side diagnostics config remains |
| `src/util/CMakeLists.txt` | Modified | Remove `goggles_diagnostics` target and `goggles_util_logging_obj` bundling into filter chain |
| `tests/CMakeLists.txt` | Modified | Remove ~22 contract test sources; keep 3 host integration tests |
| `CMakePresets.json` | Modified | Remove `.shared` and `test-shared` presets after package validation proves both modes |
| `src/render/backend/filter_chain_controller.cpp` | Modified | Switch from in-tree chain includes to installed package headers |
| `src/render/backend/filter_chain_controller.hpp` | Modified | Switch includes |
| `src/render/backend/vulkan_backend.cpp` | Modified | Switch includes |

## Impacted OpenSpec Specs

| Spec | Impact |
|------|--------|
| `goggles-filter-chain` | Validates: Standalone Filter Library Target, Library-Owned Support Boundary, Installed Public-Surface Verification Boundary requirements are now implementable and verifiable |
| `build-system` | Validates: CMake-First Standalone Filter Project Workflow, Goggles External Dependency Primary Path, Paired Static and Shared Package Outputs requirements |
| `filter-chain-c-api` | Validates: Installed C ABI Consumer Contract, Post-Retarget Output Contract requirements outside Goggles tree |
| `filter-chain-cpp-wrapper` | Validates: Installed Wrapper Consumer Contract, Wrapper Retarget Contract Survives Standalone Packaging requirements |
| `filter-chain-assets-package` | Validates: All 3 requirements (Library-Owned Asset Package, Asset Resolution Is Package-Oriented, Assets Support Public-Surface Validation) |
| `diagnostics` | Affected by: Ownership move of diagnostics implementation; diagnostic event model, sinks, session, and ledger contracts are preserved but built from standalone project |
| `render-pipeline` | Affected by: Shader runtime, preset parser, and compile report contracts now built from standalone project; host pipeline integration switches to package consumption |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Diagnostics decoupling surfaces hidden `goggles_util` type dependencies (config types, job_system references) | Medium | Audit all 10 diagnostics headers for type-level coupling before moving. The exploration found the `goggles_util` link is primarily include-path-based, not type-based, but edge cases may exist in `diagnostic_policy.hpp` (config mapping) and `gpu_timestamp_pool.hpp`. |
| Include path migration breaks compilation in non-obvious ways (template instantiation, ADL, forward declarations) | Medium | Maintain compilation at each phase boundary. Use `pixi run build -p quality` (clang-tidy as errors) as the gate after every file move batch. |
| Asset path changes break tests that use `GOGGLES_SOURCE_DIR` compile definition for fixture lookup | High | Phase 4 explicitly addresses this. Define a `FILTER_CHAIN_ASSET_DIR` compile definition pointing to the standalone asset root. Update all ~22 moved tests to use the new path. |
| Goggles build breaks during transition when chain/shader/texture sources are moved but `find_package()` is not yet configured | Medium | Use `add_subdirectory(filter-chain/)` as the transitional bridge (Phase 3). Switch to `find_package()` only in Phase 5 after install/export is proven. |
| Shared library symbol visibility issues when diagnostics and support shims are compiled into the filter-chain SHARED target | Low | The existing `GOGGLES_CHAIN_BUILD_SHARED` / `POSITION_INDEPENDENT_CODE` pattern already handles this for chain/shader/texture OBJECT libraries. Apply the same pattern to diagnostics and support objects. |
| Curated shader subset is insufficient for zfast integration and shader validation tests | Low | Start with the exact files referenced by the moved tests. Add files incrementally if tests fail. The exploration inventoried 3 asset categories with specific file lists. |
| Transitional `add_subdirectory()` period allows re-coupling (new Goggles code adds `#include <render/chain/...>` paths) | Low | The Phase 1-2 boundary contract tests already prevent this. Keep those tests active during the transition. The filter-chain target's include directories will only expose installed-surface paths. |

## Policy-Sensitive Impacts

- **Error handling**: The standalone project preserves the `tl::expected<T, Error>` / `Result<T>`
  pattern. The library-owned `error.hpp` already exists under `include/goggles/filter_chain/`.
  No change to error handling policy.

- **Logging**: The library-owned logging shim wraps spdlog with the same facade contract as
  `util/logging.hpp`. Log tags (`render.chain`, `render.shader`, `render.texture`) are preserved.
  No silent failure paths introduced.

- **Threading**: The filter chain is single-threaded by design. The diagnostics decoupling removes
  the transitive `BS_thread_pool` and `Threads` dependencies, which is correct -- the filter chain
  does not use `JobSystem`. No threading policy change.

- **Vulkan API split**: All `vk::` usage is preserved. The standalone project links
  `Vulkan::Vulkan` PUBLIC and defines `VULKAN_HPP_NO_EXCEPTIONS` and
  `VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1` as it does today.

- **Lifetime/ownership**: RAII patterns preserved. No raw `new`/`delete` introduced. The
  standalone project owns its support code lifetime; Goggles consumes through the installed
  package boundary.

## Rollback Plan

Each phase is independently revertable:

1. **Phase 3** (skeleton + source move): Revert the file moves and restore `src/render/chain/`,
   `shader/`, `texture/` CMakeLists. The `filter-chain/` directory can be deleted. Goggles builds
   as before.

2. **Phase 4** (assets + diagnostics): Revert diagnostics source moves back to
   `src/util/diagnostics/`. Restore `goggles_util` link. Revert asset path changes in tests.

3. **Phase 5** (install/export + find_package switch): Revert Goggles back to
   `add_subdirectory(filter-chain/)` or restore the in-tree chain target. Re-add `.shared` and
   `test-shared` presets if removed.

4. **Phase 6** (verification): No destructive changes -- this phase only validates.

At any point, `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality` must
pass. If it fails after a phase, revert that phase before investigating.

## Dependencies

- Phase 1-2 monorepo groundwork is complete (archived at
  `openspec/changes/archive/2026-03-13-extract-filter-chain-standalone-project/`).
- Public header boundary under `src/render/chain/include/goggles/filter_chain/` is established.
- `GogglesFilterChain::goggles-filter-chain` ALIAS target exists.
- All third-party dependencies (Vulkan, expected-lite, spdlog, slang, stb_image, Catch2) are
  available via standard `find_package()` through the Pixi/conda environment.

## Success Criteria

- [ ] `cmake -S filter-chain/ -B filter-chain/build` configures without errors and without
  requiring Goggles source tree, Pixi wrappers, or Conda-specific paths.
- [ ] `cmake --build filter-chain/build` produces both STATIC and SHARED library artifacts.
- [ ] `ctest --test-dir filter-chain/build` passes all ~22 contract tests using library-owned
  assets and fixtures.
- [ ] `cmake --install filter-chain/build --prefix /tmp/fc-install` installs headers, libraries,
  assets, and package metadata.
- [ ] An out-of-tree consumer project successfully resolves
  `find_package(GogglesFilterChain CONFIG REQUIRED)` and links both static and shared variants.
- [ ] Goggles builds and passes host integration tests when consuming the installed package
  through `find_package(GogglesFilterChain)`.
- [ ] `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality` passes
  for the Goggles project after the switch.
- [ ] The standalone `filter-chain/` directory contains no `#include <util/...>` or
  `#include <render/...>` paths that assume the Goggles source layout.
- [ ] The `goggles_diagnostics` target in the standalone project does not link `goggles_util`,
  `toml11`, `BS_thread_pool`, or `Threads`.
- [ ] The transitional `.shared` and `test-shared` presets are removed from `CMakePresets.json`.

## Validation Plan

### Per-Phase Gates

| Phase | Gate Command | What It Proves |
|-------|-------------|----------------|
| Phase 3 | `cmake -S filter-chain/ -B /tmp/fc-build && cmake --build /tmp/fc-build` | Standalone project configures and builds without Goggles tree |
| Phase 3 | `pixi run build -p asan && pixi run test -p asan` | Goggles still builds and passes tests via `add_subdirectory()` bridge |
| Phase 4 | `ctest --test-dir /tmp/fc-build` | Contract tests pass with library-owned assets |
| Phase 4 | `pixi run build -p quality` | No clang-tidy regressions in remaining Goggles code |
| Phase 5 | `cmake --install /tmp/fc-build --prefix /tmp/fc-prefix && cmake -S consumer/ -B /tmp/consumer-build -DCMAKE_PREFIX_PATH=/tmp/fc-prefix` | Installed package is discoverable and consumable |
| Phase 5 | `pixi run build -p asan && pixi run test -p asan` (with find_package path) | Goggles works with external package consumption |
| Phase 6 | Full CI-parity gate: `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality` | End-to-end pass |

### Boundary Verification

- Grep `filter-chain/src/` for `#include <util/` and `#include <render/` -- must return zero matches.
- Inspect `filter-chain/CMakeLists.txt` link dependencies -- must not reference `goggles_util`.
- Run `cmake --graphviz=deps.dot` on the standalone project -- no edges to Goggles targets.
