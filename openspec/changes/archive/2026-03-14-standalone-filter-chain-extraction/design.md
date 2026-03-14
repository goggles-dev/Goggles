# Design: Standalone Filter-Chain Extraction

## Technical Approach

The filter-chain library is already architecturally self-contained after Phase 1-2 groundwork: it
owns chain orchestration, shader runtime, texture loading, diagnostics, and the public C/C++
boundary contract through a composition of 5 OBJECT libraries into a single `goggles-filter-chain`
target with a `GogglesFilterChain::goggles-filter-chain` ALIAS. What remains is structural: severing
the 13+ file dependency on `util/logging.hpp`, `util/profiling.hpp`, and `util/serializer.hpp`;
decoupling the `goggles_diagnostics` OBJECT library from `goggles_util`; creating a standalone CMake
project at `filter-chain/`; moving sources, tests, and assets under library ownership; adding
install/export rules; and switching Goggles to package-first consumption.

The extraction is implemented as an in-repo subdirectory (`filter-chain/`) per the key decision from
the archived Phase 1-2 design. This keeps everything in a single git history for review, allows
atomic commits that move files and update consumers together, and produces a provably standalone
CMake project ready for future `git subtree split`.

The design follows the proposal's phased strategy (Phases 3-6), with each phase producing a
verifiable intermediate state. Each phase has a concrete build gate that proves correctness before
proceeding.

## Architecture Decisions

### Decision: Single composite library target with internal OBJECT modules

**Choice**: The standalone `filter-chain/CMakeLists.txt` defines 6 internal OBJECT libraries
(`fc_chain_obj`, `fc_shader_obj`, `fc_texture_obj`, `fc_diagnostics_obj`, `fc_support_obj`,
`fc_logging_obj`) composed into one `goggles-filter-chain` library target (STATIC or SHARED).
This mirrors the existing composition model in `src/render/CMakeLists.txt`.

**Alternatives considered**:
1. *Separate installed targets per module* (e.g., `GogglesFilterChain::chain`,
   `GogglesFilterChain::shader`): Exposes internal module boundaries to consumers. The public
   contract is a single library; splitting targets would complicate the package surface and invite
   cherry-picking internal modules.
2. *Single flat source list without OBJECT intermediaries*: Loses per-module log tags
   (`GOGGLES_LOG_TAG="render.chain"` vs `"render.shader"`), per-module compile definitions, and
   per-module clang-tidy configuration.

**Rationale**: The OBJECT composition model is already proven in-tree. Per-module OBJECT libraries
preserve log tag differentiation, targeted compile definitions, and compile-time isolation without
exposing module boundaries to consumers. The composition into a single exported target matches the
existing `GogglesFilterChain::goggles-filter-chain` contract.

### Decision: Library-owned support shims replace util/ facades

**Choice**: Create 3 shim files under `filter-chain/src/support/`:
- `logging.hpp` + `logging.cpp`: wraps spdlog with the same `GOGGLES_LOG_*` macro contract
- `profiling.hpp`: wraps Tracy with the same `GOGGLES_PROFILE_*` macro contract (header-only)
- `serializer.hpp`: provides `BinaryWriter`/`BinaryReader` + `read_file_binary()` (header-only)

Each shim replicates the interface contract of the corresponding `util/` header while depending
only on its own third-party dependency. The error types (`ErrorCode`, `Error`, `Result<T>`,
`GOGGLES_TRY`, `GOGGLES_MUST`) are already library-owned under
`include/goggles/filter_chain/error.hpp` from Phase 1-2 work.

**Alternatives considered**:
1. *Extract util/ as a shared utility package*: Overengineered -- only 3 thin facades are needed.
   The filter chain does not use config, job_system, paths, or any other util/ code.
2. *Keep depending on goggles_util at build time via add_subdirectory()*: Defeats the standalone
   goal. The library cannot configure without the Goggles tree.
3. *Conditional compilation switching between util/ and library-owned shims*: Unnecessary
   complexity. After sources move, no TU compiles against both.

**Rationale**: The shims are small (logging.hpp is 57 lines, profiling.hpp is 41 lines,
serializer.hpp is 153 lines). Copying these interface contracts is cheaper and safer than
maintaining a shared dependency. The `#ifndef GOGGLES_ERROR_TYPES_DEFINED` guard on the existing
library-owned `error.hpp` already handles the ODR boundary.

### Decision: Diagnostics decoupling by severing the goggles_util link

**Choice**: Move all 4 `.cpp` files and ~20 headers from `src/util/diagnostics/` to
`filter-chain/src/diagnostics/`. Replace the `goggles_util` PUBLIC link with direct dependencies:
`Vulkan::Vulkan` (for `gpu_timestamp_pool`), the library-owned `error.hpp` (for `Result<T>`), and
`spdlog::spdlog` (for `log_sink`). Remove the transitive pull of `toml11`, `BS_thread_pool`, and
`Threads`.

**Alternatives considered**:
1. *Keep diagnostics in src/util/ and link into filter-chain as an external dependency*: The
   diagnostics code is used exclusively by the filter chain. Keeping it in util/ perpetuates a
   misplaced ownership boundary.
2. *Create a separate goggles_diagnostics package*: Only one consumer (filter-chain) exists. A
   separate package adds complexity without benefit.

**Rationale**: Audit of all diagnostics headers confirms the `goggles_util` link exists because
`goggles_diagnostics` is built under `src/util/` and uses `${CMAKE_SOURCE_DIR}/src` include paths
-- not because it depends on config, job_system, or toml11 types. The only actual coupling is:
- `gpu_timestamp_pool.hpp` includes `<util/error.hpp>` -- replaced by the library-owned
  `<goggles/filter_chain/error.hpp>`
- `log_sink.cpp` uses spdlog -- linked directly as PRIVATE dependency
- `diagnostic_policy.hpp`, `diagnostic_session.hpp`, `diagnostic_event.hpp`, etc. are
  self-contained (no goggles_util types)

### Decision: Asset package layout with three categories

**Choice**: Create `filter-chain/assets/` with three subdirectories:
```
filter-chain/assets/
  shaders/test/            # Test shader fixtures (moved from shaders/retroarch/test/)
  shaders/upstream/        # Curated upstream subset (crt-lottes-fast and dependencies)
  diagnostics/             # Authoring corpus + semantic probes (moved from tests/util/test_data/)
```

Tests resolve assets via a `FILTER_CHAIN_ASSET_DIR` compile definition pointing to this root.
Installed assets go to `<prefix>/share/goggles-filter-chain/assets/`.

**Alternatives considered**:
1. *Flat asset directory*: Loses category distinction, harder to audit what comes from upstream vs
   what is library-authored.
2. *Symlinks to Goggles assets during transition*: Breaks standalone build. The standalone project
   must own all assets it needs.
3. *Ship the full shaders/retroarch/ mirror*: Includes ~200+ presets the library does not test.
   The curated subset contains only the files referenced by moved tests.

**Rationale**: Three categories match the three distinct asset sources identified in the
exploration. The compile definition approach (`FILTER_CHAIN_ASSET_DIR`) is consistent with the
existing `GOGGLES_SOURCE_DIR` pattern and works from both build tree and install prefix. The curated
upstream subset is intentionally minimal -- exactly the files needed by `test_zfast_integration.cpp`
(crt-lottes-fast.slangp and its dependencies).

### Decision: Config-file CMake package export model

**Choice**: Export via `GogglesFilterChainConfig.cmake.in` template using CMake's install(EXPORT)
and configure_package_config_file(). Three exported targets:
- `GogglesFilterChain::goggles-filter-chain` (the canonical consumer target)
- `GogglesFilterChain::goggles-filter-chain-static` (explicit STATIC validation)
- `GogglesFilterChain::goggles-filter-chain-shared` (explicit SHARED validation)

The config file includes `FilterChainDependencies.cmake` to resolve transitive PUBLIC dependencies
(Vulkan, expected-lite) before defining imported targets. PRIVATE dependencies (spdlog, slang,
stb_image) are not forwarded.

**Alternatives considered**:
1. *find_package() component model*: Components are for optional subsystems. The filter chain is a
   single cohesive library.
2. *Export only the primary target, no explicit static/shared*: Consumer validation requires
   explicit proof of both linkage modes. Without named targets, the test project cannot force a
   specific variant.
3. *pkg-config only*: Does not support imported targets, transitive dependency forwarding, or
   CMake-native consumption.

**Rationale**: The config-file package is the standard CMake approach for installed library
consumption. Including `FilterChainDependencies.cmake` from the config template ensures transitive
PUBLIC deps (Vulkan, expected-lite) are resolved before target definition, which is required for
correct imported target usage. The explicit static/shared targets exist only for validation -- the
canonical consumer target adapts to whatever variant is installed.

### Decision: Goggles transition from add_subdirectory() to find_package()

**Choice**: Phase 3 uses `add_subdirectory(filter-chain/)` as a transitional bridge. Phase 5
switches to `find_package(GogglesFilterChain CONFIG REQUIRED)` as the primary path.
`add_subdirectory()` remains available as an optional local-development convenience via a CMake
option (e.g., `GOGGLES_USE_BUNDLED_FILTER_CHAIN`).

**Alternatives considered**:
1. *Jump directly to find_package() in Phase 3*: Impossible -- install/export rules are not ready
   until Phase 5. Goggles would break during the intermediate phases.
2. *Remove add_subdirectory() entirely after switch*: Loses the side-by-side development
   convenience for iterating on filter-chain changes within the Goggles tree.
3. *Always use add_subdirectory() and defer find_package() to a future change*: Does not prove the
   installed package contract, which is the core success criterion.

**Rationale**: The transitional bridge maintains continuous build integrity while sources are moved.
The CMake option pattern (`GOGGLES_USE_BUNDLED_FILTER_CHAIN`) is a well-established convention for
"vendored vs system" dependency selection.

## Data Flow

### Standalone project build, install, and consumption flow

```
filter-chain/
  CMakeLists.txt
  cmake/
    FilterChainDependencies.cmake
    GogglesFilterChainConfig.cmake.in
    CompilerConfig.cmake
    CodeQuality.cmake
  include/
    goggles_filter_chain.h
    goggles_filter_chain.hpp
    goggles/filter_chain/*.hpp
  src/
    chain/          (moved from src/render/chain/)
    shader/         (moved from src/render/shader/)
    texture/        (moved from src/render/texture/)
    diagnostics/    (moved from src/util/diagnostics/)
    support/        (library-owned logging, profiling, serializer shims)
  tests/
    contract/       (~22 moved contract test files)
    consumer/       (out-of-tree find_package() validation projects)
  assets/
    shaders/test/       (moved from shaders/retroarch/test/)
    shaders/upstream/   (curated crt-lottes-fast subset)
    diagnostics/        (moved from tests/util/test_data/filter_chain_diagnostics/)

cmake configure (cmake -S filter-chain/ -B build)
  -> FilterChainDependencies.cmake resolves Vulkan, expected-lite, spdlog, slang, stb_image
  -> Define OBJECT libraries: fc_chain_obj, fc_shader_obj, fc_texture_obj,
     fc_diagnostics_obj, fc_support_obj, fc_logging_obj
  -> Compose goggles-filter-chain target (STATIC or SHARED)
  -> Build test executable from tests/contract/ sources

cmake install (cmake --install build --prefix <prefix>)
  -> <prefix>/include/           (public headers)
  -> <prefix>/lib/               (libgoggles-filter-chain.a and/or .so)
  -> <prefix>/lib/cmake/GogglesFilterChain/   (package config + targets)
  -> <prefix>/share/goggles-filter-chain/assets/  (installed assets)
```

### Goggles consumption after switch

```
Goggles CMakeLists.txt
  -> find_package(GogglesFilterChain CONFIG REQUIRED)
     -> GogglesFilterChainConfig.cmake
        -> include(FilterChainDependencies.cmake)   # resolve Vulkan, expected-lite
        -> include(GogglesFilterChainTargets.cmake)  # define imported targets
  -> target_link_libraries(goggles_render PUBLIC GogglesFilterChain::goggles-filter-chain)

Goggles backend code
  -> #include <goggles_filter_chain.hpp>                          (installed C++ wrapper)
  -> #include <goggles/filter_chain/filter_controls.hpp>          (installed public header)
  -> #include <goggles/filter_chain/vulkan_context.hpp>           (installed public header)
  -> Resolved from installed package include paths, not source tree
```

### Include path remapping for moved sources

```
BEFORE (in-tree):                          AFTER (standalone):
  #include <util/logging.hpp>           -> #include "support/logging.hpp"
  #include <util/profiling.hpp>         -> #include "support/profiling.hpp"
  #include <util/serializer.hpp>        -> #include "support/serializer.hpp"
  #include <util/error.hpp>             -> #include <goggles/filter_chain/error.hpp>
  #include <util/diagnostics/X.hpp>     -> #include "diagnostics/X.hpp"
  #include <render/chain/X.hpp>         -> #include "chain/X.hpp"
  #include <render/shader/X.hpp>        -> #include "shader/X.hpp"
```

## File Changes

### New files (standalone project skeleton)

| File | Action | Description |
|------|--------|-------------|
| `filter-chain/CMakeLists.txt` | Create | Top-level standalone project definition: project(), option for STATIC/SHARED, OBJECT library composition, install/export rules, test enablement |
| `filter-chain/cmake/FilterChainDependencies.cmake` | Create | Resolves Vulkan, expected-lite, spdlog, slang, stb_image, Catch2 via standard find_package() |
| `filter-chain/cmake/GogglesFilterChainConfig.cmake.in` | Create | Package config template: includes FilterChainDependencies for transitive deps, includes targets export file |
| `filter-chain/cmake/CompilerConfig.cmake` | Create | C++20 enforcement, ccache, warning flags, sanitizer helpers (subset of goggles cmake/CompilerConfig.cmake) |
| `filter-chain/cmake/CodeQuality.cmake` | Create | clang-tidy helper function (subset of goggles cmake/CodeQuality.cmake) |
| `filter-chain/.clang-format` | Create | Symlink or copy of root `.clang-format` for standalone formatting |
| `filter-chain/.clang-tidy` | Create | Symlink or copy of root `.clang-tidy` for standalone linting |

### New files (library-owned support shims)

| File | Action | Description |
|------|--------|-------------|
| `filter-chain/src/support/logging.hpp` | Create | spdlog facade with GOGGLES_LOG_* macros matching util/logging.hpp contract |
| `filter-chain/src/support/logging.cpp` | Create | Logger initialization and singleton, matching util/logging.cpp |
| `filter-chain/src/support/profiling.hpp` | Create | Tracy facade with GOGGLES_PROFILE_* macros matching util/profiling.hpp contract (header-only) |
| `filter-chain/src/support/serializer.hpp` | Create | BinaryWriter/BinaryReader + read_file_binary() matching util/serializer.hpp contract (header-only) |

### New files (consumer validation)

| File | Action | Description |
|------|--------|-------------|
| `filter-chain/tests/consumer/static/CMakeLists.txt` | Create | Out-of-tree consumer that links GogglesFilterChain::goggles-filter-chain-static |
| `filter-chain/tests/consumer/static/main.cpp` | Create | Minimal consumer: includes public headers, instantiates types, verifies linkage |
| `filter-chain/tests/consumer/shared/CMakeLists.txt` | Create | Out-of-tree consumer that links GogglesFilterChain::goggles-filter-chain-shared |
| `filter-chain/tests/consumer/shared/main.cpp` | Create | Minimal consumer: same as static variant |

### Moved files (source migration)

| File | Action | Description |
|------|--------|-------------|
| `src/render/chain/*.cpp` (14 files) | Move | `chain_runtime.cpp`, `chain_builder.cpp`, `chain_resources.cpp`, `chain_executor.cpp`, `chain_controls.cpp`, `filter_controls.cpp`, `filter_pass.cpp`, `framebuffer.cpp`, `output_pass.cpp`, `downsample_pass.cpp`, `preset_parser.cpp`, `frame_history.cpp`, `vulkan_dispatch.cpp`, `api/c/goggles_filter_chain.cpp`, `api/cpp/goggles_filter_chain.cpp` -> `filter-chain/src/chain/` |
| `src/render/chain/*.hpp` (16 files) | Move | All private headers (`chain_runtime.hpp`, `chain_builder.hpp`, `chain_resources.hpp`, `chain_executor.hpp`, `chain_controls.hpp`, `filter_pass.hpp`, `framebuffer.hpp`, `output_pass.hpp`, `downsample_pass.hpp`, `preset_parser.hpp`, `frame_history.hpp`, `debug_label_scope.hpp`, `pass.hpp`, `semantic_binder.hpp`, `vulkan_result.hpp`) -> `filter-chain/src/chain/` |
| `src/render/chain/include/goggles/filter_chain/*.hpp` (5 files) | Move | `error.hpp`, `filter_controls.hpp`, `result.hpp`, `scale_mode.hpp`, `vulkan_context.hpp` -> `filter-chain/include/goggles/filter_chain/` |
| `src/render/chain/api/c/goggles_filter_chain.h` | Move | -> `filter-chain/include/goggles_filter_chain.h` |
| `src/render/chain/api/cpp/goggles_filter_chain.hpp` | Move | -> `filter-chain/include/goggles_filter_chain.hpp` |
| `src/render/shader/*.cpp` (3 files) | Move | `shader_runtime.cpp`, `retroarch_preprocessor.cpp`, `slang_reflect.cpp` -> `filter-chain/src/shader/` |
| `src/render/shader/*.hpp` (3 files) | Move | `shader_runtime.hpp`, `retroarch_preprocessor.hpp`, `slang_reflect.hpp` -> `filter-chain/src/shader/` |
| `src/render/texture/texture_loader.cpp` | Move | -> `filter-chain/src/texture/` |
| `src/render/texture/texture_loader.hpp` | Move | -> `filter-chain/src/texture/` |
| `src/util/diagnostics/*.cpp` (4 files) | Move | `log_sink.cpp`, `test_harness_sink.cpp`, `diagnostic_session.cpp`, `gpu_timestamp_pool.cpp` -> `filter-chain/src/diagnostics/` |
| `src/util/diagnostics/*.hpp` (~20 files) | Move | All diagnostics headers -> `filter-chain/src/diagnostics/` |

### Moved files (test migration -- ~22 contract test files)

| File | Action | Description |
|------|--------|-------------|
| `tests/render/test_filter_chain.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_filter_chain_c_api_contracts.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_filter_chain_retarget_contract.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_filter_controls.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_preset_parser.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_retroarch_preprocessor.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_slang_reflect.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_shader_runtime.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_semantic_binder.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_zfast_integration.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_shader_validation.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_runtime_diagnostics.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_diagnostic_event_model.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_diagnostic_sinks.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_binding_ledger.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_diagnostic_ledgers.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_diagnostic_session.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_diagnostic_reporting.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_source_provenance.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_compile_report.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_authoring_validation.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_gpu_timestamp_pool.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/test_shader_batch_report.cpp` | Move | -> `filter-chain/tests/contract/` |
| `tests/render/shader_batch_report.cpp` | Move | -> `filter-chain/tests/contract/` (test helper) |
| `tests/render/shader_batch_report.hpp` | Move | -> `filter-chain/tests/contract/` (test helper) |
| `tests/render/shader_batch_report_main.cpp` | Move | -> `filter-chain/tests/contract/` (batch report tool) |

### Moved files (asset migration)

| File | Action | Description |
|------|--------|-------------|
| `shaders/retroarch/test/*` (13 files) | Copy | -> `filter-chain/assets/shaders/test/` (Goggles retains original for its own tests) |
| `tests/util/test_data/filter_chain_diagnostics/*` (13 files) | Move | -> `filter-chain/assets/diagnostics/` |
| `shaders/retroarch/crt/crt-lottes-fast.*` + deps | Copy | -> `filter-chain/assets/shaders/upstream/crt/` (curated subset for zfast tests) |

### Modified files (Goggles integration)

| File | Action | Description |
|------|--------|-------------|
| `src/render/CMakeLists.txt` | Modify | Replace chain/shader/texture OBJECT library definitions and goggles-filter-chain composition with `find_package(GogglesFilterChain)` or `add_subdirectory(filter-chain/)`. Remove in-tree chain/shader/texture `add_subdirectory()` calls. |
| `src/render/chain/CMakeLists.txt` | Remove | Sources moved to standalone project |
| `src/render/shader/CMakeLists.txt` | Remove | Sources moved to standalone project |
| `src/render/texture/CMakeLists.txt` | Remove | Sources moved to standalone project |
| `src/util/CMakeLists.txt` | Modify | Remove `goggles_diagnostics` target definition. Remove `goggles_util_logging_obj` inclusion in filter-chain composition. Keep `goggles_util_logging_obj` for `goggles_util` itself. Remove `add_subdirectory(diagnostics)`. |
| `src/util/diagnostics/CMakeLists.txt` | Remove | Sources moved to standalone project |
| `tests/CMakeLists.txt` | Modify | Remove ~22 contract test source references from `goggles_tests`. Keep 3 host integration tests. Keep shader_batch_report tool if it stays in Goggles. |
| `CMakePresets.json` | Modify | Remove `.shared` hidden preset and `test-shared` configure/build/test presets after package validation proves both linkage modes |
| `src/render/backend/filter_chain_controller.hpp` | Modify | Includes already use installed-surface paths (`<goggles_filter_chain.hpp>`, `<goggles/filter_chain/*.hpp>`). Remove `<util/config.hpp>` if the config type reference can be decoupled. |
| `src/render/backend/filter_chain_controller.cpp` | Modify | Includes already use `<util/logging.hpp>` and `<util/profiling.hpp>` -- these stay (they are Goggles-owned host code, not filter-chain library code). No change needed. |
| `CMakeLists.txt` | Modify | Add `option(GOGGLES_USE_BUNDLED_FILTER_CHAIN ...)` and conditional `add_subdirectory(filter-chain/)` or `find_package(GogglesFilterChain)` |

### Files NOT changed

| File | Reason |
|------|--------|
| `src/render/backend/vulkan_backend.cpp` | Already uses only Goggles-local `util/` includes. Not part of filter-chain library. |
| `src/render/backend/vulkan_backend.hpp` | Includes are backend-specific. No filter-chain include changes needed. |
| `src/util/logging.hpp` | Goggles retains its own copy for non-filter-chain code. |
| `src/util/profiling.hpp` | Goggles retains its own copy for non-filter-chain code. |
| `src/util/serializer.hpp` | Goggles retains its own copy for non-filter-chain code. |
| `src/util/error.hpp` | Goggles retains its own copy. The `#ifndef GOGGLES_ERROR_TYPES_DEFINED` guard prevents ODR violations. |
| `shaders/retroarch/` (full mirror) | Not moved. Goggles retains the full upstream mirror. |
| `tests/render/test_filter_boundary_contracts.cpp` | Host integration test -- stays in Goggles |
| `tests/render/test_vulkan_backend_subsystem_contracts.cpp` | Host integration test -- stays in Goggles |
| `tests/render/test_filter_chain_retarget.cpp` | Host integration test -- stays in Goggles |

## Interfaces / Contracts

### Package export model

Consumers use:
```cmake
find_package(GogglesFilterChain CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE GogglesFilterChain::goggles-filter-chain)
```

Exported targets:

| Target | Purpose |
|--------|---------|
| `GogglesFilterChain::goggles-filter-chain` | Canonical consumer target (resolves to installed STATIC or SHARED) |
| `GogglesFilterChain::goggles-filter-chain-static` | Explicit STATIC target for validation |
| `GogglesFilterChain::goggles-filter-chain-shared` | Explicit SHARED target for validation |

Transitive dependencies forwarded through package config:
- PUBLIC: `Vulkan::Vulkan`, `nonstd::expected-lite`
- PRIVATE (not forwarded): `spdlog::spdlog`, `slang::slang`, `stb_image`

### Asset resolution contract

Build-tree tests:
```cmake
target_compile_definitions(fc_tests PRIVATE
    FILTER_CHAIN_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
```

Installed-surface tests:
```cmake
target_compile_definitions(fc_tests PRIVATE
    FILTER_CHAIN_ASSET_DIR="${CMAKE_INSTALL_PREFIX}/share/goggles-filter-chain/assets")
```

Test code resolves fixtures as:
```cpp
auto preset = std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders/test/format.slangp";
```

### Support shim interface contracts

**Logging shim** (`filter-chain/src/support/logging.hpp`):
- Provides: `goggles::initialize_logger()`, `goggles::get_logger()`, `goggles::set_log_level()`
- Provides: `GOGGLES_LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL` macros with `GOGGLES_LOG_TAG`
- Depends on: `spdlog::spdlog`

**Profiling shim** (`filter-chain/src/support/profiling.hpp`):
- Provides: `GOGGLES_PROFILE_FRAME/FUNCTION/SCOPE/TAG/VALUE` macros
- Depends on: Tracy when `TRACY_ENABLE` is defined; no-op otherwise

**Serializer shim** (`filter-chain/src/support/serializer.hpp`):
- Provides: `goggles::util::BinaryWriter`, `goggles::util::BinaryReader`, `goggles::util::read_file_binary()`
- Depends on: standard library + `goggles/filter_chain/error.hpp`

### Goggles integration option

```cmake
# In root CMakeLists.txt or src/render/CMakeLists.txt
option(GOGGLES_USE_BUNDLED_FILTER_CHAIN
    "Build filter-chain from in-repo subdirectory instead of finding installed package" OFF)

if(GOGGLES_USE_BUNDLED_FILTER_CHAIN)
    add_subdirectory(${CMAKE_SOURCE_DIR}/filter-chain)
else()
    find_package(GogglesFilterChain CONFIG REQUIRED)
endif()

# Either path makes GogglesFilterChain::goggles-filter-chain available
target_link_libraries(goggles_render PUBLIC GogglesFilterChain::goggles-filter-chain)
```

### Standalone project CMakeLists.txt structure (key sections)

```cmake
cmake_minimum_required(VERSION 3.20)
project(GogglesFilterChain VERSION 0.1.0 LANGUAGES CXX)

# Options
set(FILTER_CHAIN_LIBRARY_TYPE "STATIC" CACHE STRING "Library type (STATIC or SHARED)")
option(FILTER_CHAIN_BUILD_TESTS "Build tests" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

# CMake modules
include(cmake/CompilerConfig.cmake)
include(cmake/CodeQuality.cmake)
include(cmake/FilterChainDependencies.cmake)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# OBJECT libraries with per-module log tags
add_library(fc_chain_obj OBJECT ...)
target_compile_definitions(fc_chain_obj PRIVATE GOGGLES_LOG_TAG="render.chain")

add_library(fc_shader_obj OBJECT ...)
target_compile_definitions(fc_shader_obj PRIVATE GOGGLES_LOG_TAG="render.shader")

add_library(fc_texture_obj OBJECT ...)
target_compile_definitions(fc_texture_obj PRIVATE GOGGLES_LOG_TAG="render.texture")

add_library(fc_diagnostics_obj OBJECT ...)
add_library(fc_support_obj OBJECT ...)   # logging.cpp only
add_library(fc_logging_obj OBJECT ...)   # => merged into fc_support_obj

# Composed library
add_library(goggles-filter-chain ${FILTER_CHAIN_LIBRARY_TYPE}
    $<TARGET_OBJECTS:fc_chain_obj>
    $<TARGET_OBJECTS:fc_shader_obj>
    $<TARGET_OBJECTS:fc_texture_obj>
    $<TARGET_OBJECTS:fc_diagnostics_obj>
    $<TARGET_OBJECTS:fc_support_obj>
)

# Include directories: only standalone-relative paths
target_include_directories(goggles-filter-chain
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Public compile definitions
target_compile_definitions(goggles-filter-chain PUBLIC
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

# Link libraries
target_link_libraries(goggles-filter-chain
    PUBLIC  Vulkan::Vulkan nonstd::expected-lite
    PRIVATE spdlog::spdlog slang::slang stb_image
)

# ALIAS for add_subdirectory() consumers
add_library(GogglesFilterChain::goggles-filter-chain ALIAS goggles-filter-chain)

# Install rules (Phase 5)
install(TARGETS goggles-filter-chain EXPORT GogglesFilterChainTargets ...)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY assets/ DESTINATION share/goggles-filter-chain/assets)
install(EXPORT GogglesFilterChainTargets NAMESPACE GogglesFilterChain:: ...)

# Tests
if(FILTER_CHAIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit (standalone) | Preset parser, preprocessor, reflection, control helpers, diagnostics models, ledgers, sessions, sinks, source provenance, compile report | ~22 contract test files under `filter-chain/tests/contract/`, compiled against library-owned headers and linked to `goggles-filter-chain` target. Asset paths resolved via `FILTER_CHAIN_ASSET_DIR`. |
| Integration (standalone) | C API contracts, C++ wrapper contracts, retarget contract, zfast shader integration, shader validation | Same test executable, using curated upstream shader subset from `filter-chain/assets/shaders/upstream/`. |
| Consumer validation (standalone) | Package discoverability and linkage for STATIC and SHARED | Two minimal out-of-tree CMake projects under `filter-chain/tests/consumer/` that configure, build, and link against the installed package via `find_package(GogglesFilterChain)`. Run as CTest fixtures after install. |
| Host integration (Goggles) | FilterChainController wiring, VulkanBackend subsystem contracts, filter boundary behavior | 3 test files remain in `tests/render/`: `test_filter_boundary_contracts.cpp`, `test_vulkan_backend_subsystem_contracts.cpp`, `test_filter_chain_retarget.cpp`. These compile against the installed or subdirectory-provided target. |
| Asset verification (standalone) | Packaged fixture resolution | Tested implicitly by all contract tests resolving through `FILTER_CHAIN_ASSET_DIR`. Explicit test: verify key asset files exist at expected paths. |
| Include-path isolation (CI gate) | No Goggles-layout includes in standalone tree | `grep -r '#include <util/' filter-chain/src/` and `grep -r '#include <render/' filter-chain/src/` must return zero matches. Enforced as a verification step. |

### Test executable structure (standalone)

```cmake
# filter-chain/tests/CMakeLists.txt
add_executable(fc_tests
    contract/test_filter_chain.cpp
    contract/test_filter_chain_c_api_contracts.cpp
    contract/test_filter_chain_retarget_contract.cpp
    contract/test_filter_controls.cpp
    contract/test_preset_parser.cpp
    # ... (~22 files total)
)

target_link_libraries(fc_tests PRIVATE
    goggles-filter-chain
    Catch2::Catch2WithMain
)

target_compile_definitions(fc_tests PRIVATE
    FILTER_CHAIN_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
)

target_include_directories(fc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME fc_contract_tests COMMAND fc_tests)
```

## Migration / Rollout

### Phase 3: Standalone skeleton and source migration

1. Create `filter-chain/` directory with `CMakeLists.txt`, `cmake/`, `include/`, `src/`, `tests/`.
2. Create library-owned support shims under `filter-chain/src/support/`.
3. Move (git mv) chain, shader, texture sources to `filter-chain/src/`.
4. Move public headers and API headers to `filter-chain/include/`.
5. Update all include paths in moved files from `<util/...>` and `<render/...>` to
   standalone-relative paths.
6. Update Goggles `src/render/CMakeLists.txt` to use `add_subdirectory(filter-chain/)` bridge.
7. Remove `src/render/chain/CMakeLists.txt`, `shader/CMakeLists.txt`, `texture/CMakeLists.txt`.

**Gate**: `cmake -S filter-chain/ -B /tmp/fc-build && cmake --build /tmp/fc-build` succeeds.
**Gate**: `pixi run build -p asan && pixi run test -p asan` passes (Goggles via bridge).

### Phase 4: Diagnostics and assets under library ownership

1. Move diagnostics sources and headers to `filter-chain/src/diagnostics/`.
2. Replace `goggles_util` link with direct deps (Vulkan, spdlog).
3. Update diagnostics `#include <util/error.hpp>` to `<goggles/filter_chain/error.hpp>`.
4. Create `filter-chain/assets/` with three categories.
5. Move ~22 contract test files to `filter-chain/tests/contract/`.
6. Update moved tests to use `FILTER_CHAIN_ASSET_DIR` instead of `GOGGLES_SOURCE_DIR`.
7. Remove moved test sources from Goggles `tests/CMakeLists.txt`.
8. Remove `src/util/diagnostics/CMakeLists.txt` and update `src/util/CMakeLists.txt`.

**Gate**: `ctest --test-dir /tmp/fc-build` passes all contract tests.
**Gate**: `pixi run build -p quality` passes (no clang-tidy regressions).

### Phase 5: Install, export, and package-first switch

1. Add install/export rules to `filter-chain/CMakeLists.txt`.
2. Create `GogglesFilterChainConfig.cmake.in` template.
3. Add consumer validation projects under `filter-chain/tests/consumer/`.
4. Install to test prefix and run consumer validation.
5. Switch Goggles from `add_subdirectory()` to `find_package()` with
   `GOGGLES_USE_BUNDLED_FILTER_CHAIN` option.
6. Remove `.shared` and `test-shared` presets from `CMakePresets.json`.

**Gate**: Consumer validation succeeds for both STATIC and SHARED.
**Gate**: `pixi run build -p asan && pixi run test -p asan` passes with `find_package()` path.

### Phase 6: End-to-end verification

1. Clean-checkout standalone build: configure, build, test, install from scratch.
2. Run installed-surface contract tests.
3. Run Goggles build + host integration tests against installed package.
4. Verify include-path isolation (grep audit).
5. Verify `cmake --graphviz` on standalone project shows no Goggles target edges.

**Gate**: Full CI-parity: `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`.

## Open Questions

- [ ] None. All technical decisions have been resolved through the exploration, proposal, and
  archived design. The diagnostics audit confirmed no hidden type-level coupling to goggles_util.
