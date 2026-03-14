# Tasks: Standalone Filter-Chain Extraction

## Phase 3: Standalone Skeleton + Source Migration

### 3A: Project skeleton and CMake infrastructure

- [x] 3.1 Create `filter-chain/` directory structure with subdirectories `cmake/`, `include/`,
      `include/goggles/filter_chain/`, `src/chain/`, `src/shader/`, `src/texture/`,
      `src/support/`, `src/diagnostics/`, `tests/`, `tests/contract/`, `assets/`.
      **Verify**: `ls filter-chain/{cmake,include,src,tests,assets}` succeeds.

- [x] 3.2 Create `filter-chain/cmake/CompilerConfig.cmake` with C++20 enforcement, warning flags,
      ccache support, sanitizer helpers, and `POSITION_INDEPENDENT_CODE` handling for SHARED builds.
      Adapt the relevant subset from the root `cmake/CompilerConfig.cmake`.
      **Verify**: The file defines `goggles_enable_sanitizers()` and sets `CMAKE_CXX_STANDARD 20`.

- [x] 3.3 Create `filter-chain/cmake/CodeQuality.cmake` with the `goggles_enable_clang_tidy()`
      helper function. Adapt from root `cmake/CodeQuality.cmake`.
      **Verify**: The file defines the `goggles_enable_clang_tidy()` function.

- [x] 3.4 Create `filter-chain/cmake/FilterChainDependencies.cmake` that resolves all third-party
      dependencies via standard `find_package()`: Vulkan, expected-lite, spdlog, slang, stb_image,
      and Catch2 (conditionally, only when tests are enabled). No Goggles-specific Find modules.
      **Verify**: File contains `find_package(Vulkan REQUIRED)`, `find_package(expected-lite REQUIRED)`,
      `find_package(spdlog REQUIRED)`, `find_package(slang CONFIG REQUIRED)`.

- [x] 3.5 Create `filter-chain/CMakeLists.txt` with:
      - `project(GogglesFilterChain VERSION 0.1.0 LANGUAGES CXX)`
      - Options: `FILTER_CHAIN_LIBRARY_TYPE` (STATIC/SHARED), `FILTER_CHAIN_BUILD_TESTS`,
        `ENABLE_CLANG_TIDY`, `ENABLE_ASAN`
      - `include()` calls for CompilerConfig, CodeQuality, FilterChainDependencies, GNUInstallDirs
      - Empty OBJECT library targets as placeholders: `fc_chain_obj`, `fc_shader_obj`,
        `fc_texture_obj`, `fc_support_obj` (sources added in later tasks)
      - Composed `goggles-filter-chain` library from OBJECT targets
      - PUBLIC include dirs: `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>`
      - PRIVATE include dirs: `${CMAKE_CURRENT_SOURCE_DIR}/src`
      - PUBLIC compile defs: `VULKAN_HPP_NO_EXCEPTIONS`, `VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1`
      - Link libraries: PUBLIC `Vulkan::Vulkan`, `nonstd::expected-lite`;
        PRIVATE `spdlog::spdlog`, `slang::slang`, `stb_image`
      - `GogglesFilterChain::goggles-filter-chain` ALIAS
      - SHARED/PIC handling matching existing pattern in `src/render/CMakeLists.txt`
      - Conditional `add_subdirectory(tests)` when `FILTER_CHAIN_BUILD_TESTS` is ON
      **Verify**: `cmake -S filter-chain/ -B /tmp/fc-skeleton -DFILTER_CHAIN_BUILD_TESTS=OFF`
      configures without errors (with deps available via Pixi env).

- [x] 3.6 Copy `.clang-format` and `.clang-tidy` from project root into `filter-chain/` for
      standalone formatting and linting consistency.
      **Verify**: `diff filter-chain/.clang-format .clang-format` shows no differences.

### 3B: Library-owned support shims

- [x] 3.7 Create `filter-chain/src/support/logging.hpp` with spdlog facade providing
      `goggles::initialize_logger()`, `goggles::get_logger()`, `goggles::set_log_level()`, and
      `GOGGLES_LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL` macros using `GOGGLES_LOG_TAG`.
      Replicate the interface contract from `src/util/logging.hpp`. Depend only on `spdlog::spdlog`.
      **Verify**: Header compiles standalone: include it from a trivial .cpp with spdlog available.

- [x] 3.8 Create `filter-chain/src/support/logging.cpp` with logger initialization and singleton
      management matching `src/util/logging.cpp` behavior.
      **Verify**: File compiles as part of `fc_support_obj` OBJECT library.

- [x] 3.9 Create `filter-chain/src/support/profiling.hpp` (header-only) with Tracy facade providing
      `GOGGLES_PROFILE_FRAME/FUNCTION/SCOPE/TAG/VALUE` macros. When `TRACY_ENABLE` is not defined,
      macros expand to no-ops. Replicate interface from `src/util/profiling.hpp`.
      **Verify**: Compiles with and without `TRACY_ENABLE` defined.

- [x] 3.10 Create `filter-chain/src/support/serializer.hpp` (header-only) with
      `goggles::util::BinaryWriter`, `goggles::util::BinaryReader`, and
      `goggles::util::read_file_binary()`. Depends only on standard library +
      `<goggles/filter_chain/error.hpp>`. Replicate interface from `src/util/serializer.hpp`.
      **Verify**: `shader_runtime.cpp` (once moved) can compile against this shim.

- [x] 3.11 Add `fc_support_obj` OBJECT library to `filter-chain/CMakeLists.txt` with
      `src/support/logging.cpp` as its source. Link PRIVATE `spdlog::spdlog`. Set
      `GOGGLES_LOG_TAG="render.support"`. Handle PIC for SHARED builds.
      **Verify**: `cmake --build /tmp/fc-skeleton --target fc_support_obj` succeeds.

### 3C: Source migration -- public headers and API headers

- [x] 3.12 Move (`git mv`) the 5 public headers from `src/render/chain/include/goggles/filter_chain/`
      (`error.hpp`, `filter_controls.hpp`, `result.hpp`, `scale_mode.hpp`, `vulkan_context.hpp`)
      to `filter-chain/include/goggles/filter_chain/`.
      **Verify**: `ls filter-chain/include/goggles/filter_chain/*.hpp | wc -l` returns 5.

- [x] 3.13 Move (`git mv`) the C API header `src/render/chain/api/c/goggles_filter_chain.h`
      to `filter-chain/include/goggles_filter_chain.h`.
      **Verify**: `test -f filter-chain/include/goggles_filter_chain.h`

- [x] 3.14 Move (`git mv`) the C++ wrapper header `src/render/chain/api/cpp/goggles_filter_chain.hpp`
      to `filter-chain/include/goggles_filter_chain.hpp`.
      **Verify**: `test -f filter-chain/include/goggles_filter_chain.hpp`

### 3D: Source migration -- chain module

- [x] 3.15 Move (`git mv`) all 15 chain `.cpp` files from `src/render/chain/` to
      `filter-chain/src/chain/`: `chain_runtime.cpp`, `chain_builder.cpp`, `chain_resources.cpp`,
      `chain_executor.cpp`, `chain_controls.cpp`, `filter_controls.cpp`, `filter_pass.cpp`,
      `framebuffer.cpp`, `output_pass.cpp`, `downsample_pass.cpp`, `preset_parser.cpp`,
      `frame_history.cpp`, `vulkan_dispatch.cpp`, `api/c/goggles_filter_chain.cpp` (to
      `filter-chain/src/chain/c_api.cpp`), `api/cpp/goggles_filter_chain.cpp` (to
      `filter-chain/src/chain/cpp_wrapper.cpp`).
      **Verify**: `ls filter-chain/src/chain/*.cpp | wc -l` returns 15.

- [x] 3.16 Move (`git mv`) all 15 chain private `.hpp` files from `src/render/chain/` to
      `filter-chain/src/chain/`: `chain_runtime.hpp`, `chain_builder.hpp`, `chain_resources.hpp`,
      `chain_executor.hpp`, `chain_controls.hpp`, `filter_pass.hpp`, `framebuffer.hpp`,
      `output_pass.hpp`, `downsample_pass.hpp`, `preset_parser.hpp`, `frame_history.hpp`,
      `debug_label_scope.hpp`, `pass.hpp`, `semantic_binder.hpp`, `vulkan_result.hpp`.
      Also move `README.md` if relevant.
      **Verify**: `ls filter-chain/src/chain/*.hpp | wc -l` returns 15.

- [x] 3.17 Update include paths in all moved chain source files:
      - `#include <util/logging.hpp>` -> `#include "support/logging.hpp"`
      - `#include <util/profiling.hpp>` -> `#include "support/profiling.hpp"`
      - `#include <util/error.hpp>` -> `#include <goggles/filter_chain/error.hpp>`
      - `#include <render/chain/X.hpp>` -> `#include "chain/X.hpp"`
      - `#include <render/shader/X.hpp>` -> `#include "shader/X.hpp"`
      - `#include <render/texture/X.hpp>` -> `#include "texture/X.hpp"`
      - `#include <util/diagnostics/X.hpp>` -> `#include "diagnostics/X.hpp"`
      **Verify**: `grep -r '#include <util/' filter-chain/src/chain/` returns zero matches.
      `grep -r '#include <render/' filter-chain/src/chain/` returns zero matches.

- [x] 3.18 Add `fc_chain_obj` OBJECT library sources in `filter-chain/CMakeLists.txt` with all 15
      moved `.cpp` files. Set `GOGGLES_LOG_TAG="render.chain"`. Link PRIVATE dependencies matching
      existing `goggles_render_chain_obj` target (Vulkan, slang, stb_image). Handle PIC for SHARED.
      **Verify**: The target is defined with correct source list.

### 3E: Source migration -- shader module

- [x] 3.19 Move (`git mv`) shader sources from `src/render/shader/` to `filter-chain/src/shader/`:
      `shader_runtime.cpp`, `shader_runtime.hpp`, `retroarch_preprocessor.cpp`,
      `retroarch_preprocessor.hpp`, `slang_reflect.cpp`, `slang_reflect.hpp`.
      **Verify**: `ls filter-chain/src/shader/*.{cpp,hpp} | wc -l` returns 6.

- [x] 3.20 Update include paths in moved shader files:
      - `#include <util/logging.hpp>` -> `#include "support/logging.hpp"`
      - `#include <util/profiling.hpp>` -> `#include "support/profiling.hpp"`
      - `#include <util/serializer.hpp>` -> `#include "support/serializer.hpp"`
      - `#include <util/error.hpp>` -> `#include <goggles/filter_chain/error.hpp>`
      - `#include <render/shader/X.hpp>` -> `#include "shader/X.hpp"`
      - `#include <util/diagnostics/X.hpp>` -> `#include "diagnostics/X.hpp"`
      **Verify**: `grep -r '#include <util/' filter-chain/src/shader/` returns zero matches.

- [x] 3.21 Add `fc_shader_obj` OBJECT library sources in `filter-chain/CMakeLists.txt` with the 3
      `.cpp` files. Set `GOGGLES_LOG_TAG="render.shader"`. Link PRIVATE `slang::slang`.
      Handle PIC for SHARED.
      **Verify**: Target is defined with correct source list.

### 3F: Source migration -- texture module

- [x] 3.22 Move (`git mv`) texture sources from `src/render/texture/` to
      `filter-chain/src/texture/`: `texture_loader.cpp`, `texture_loader.hpp`.
      **Verify**: `ls filter-chain/src/texture/*.{cpp,hpp} | wc -l` returns 2.

- [x] 3.23 Update include paths in moved texture files:
      - `#include <util/logging.hpp>` -> `#include "support/logging.hpp"`
      - `#include <util/profiling.hpp>` -> `#include "support/profiling.hpp"`
      - `#include <util/error.hpp>` -> `#include <goggles/filter_chain/error.hpp>`
      - `#include <render/texture/X.hpp>` -> `#include "texture/X.hpp"`
      **Verify**: `grep -r '#include <util/' filter-chain/src/texture/` returns zero matches.

- [x] 3.24 Add `fc_texture_obj` OBJECT library sources in `filter-chain/CMakeLists.txt` with
      `texture_loader.cpp`. Set `GOGGLES_LOG_TAG="render.texture"`. Link PRIVATE `stb_image`.
      Handle PIC for SHARED.
      **Verify**: Target is defined with correct source list.

### 3G: Goggles transitional bridge

- [x] 3.25 Update `src/render/CMakeLists.txt` to consume the standalone project via
      `add_subdirectory(${CMAKE_SOURCE_DIR}/filter-chain ${CMAKE_BINARY_DIR}/filter-chain)`.
      Remove the in-tree `add_subdirectory(chain)`, `add_subdirectory(shader)`,
      `add_subdirectory(texture)` calls. Remove the in-tree `goggles-filter-chain` target
      definition and OBJECT library composition (lines 8-78). Keep `goggles_render` target
      linking to `GogglesFilterChain::goggles-filter-chain` (via ALIAS from standalone).
      **Verify**: `pixi run build -p debug` succeeds.

- [x] 3.26 Remove `src/render/chain/CMakeLists.txt` (sources moved to standalone). Keep the
      `src/render/chain/` directory only if non-moved files (docs, AGENTS.md) remain.
      Remove `src/render/shader/CMakeLists.txt` and `src/render/texture/CMakeLists.txt`.
      **Verify**: The removed CMakeLists files no longer exist. No stale `add_subdirectory()`.

- [x] 3.27 Update `src/util/CMakeLists.txt`: remove the `goggles_util_logging_obj` OBJECT library
      composition into the filter-chain target (the standalone project now owns its own logging).
      Keep `goggles_util_logging_obj` for `goggles_util` itself (it is still composed into
      `goggles_util STATIC` on line 28). No changes to `goggles_util` target.
      **Verify**: `pixi run build -p debug` succeeds with the standalone bridge.

### 3H: Phase 3 build gate

- [x] 3.28 Verify standalone project configures and builds independently:
      `cmake -S filter-chain/ -B /tmp/fc-build && cmake --build /tmp/fc-build`.
      Fix any configuration or compilation errors from include path migration.
      **Verify**: Both commands exit with status 0 (tests may fail -- that is Phase 4 scope).

- [x] 3.29 Verify Goggles builds and tests pass via the transitional bridge:
      `pixi run build -p asan && pixi run test -p asan`.
      Fix any regressions from the source migration.
      **Verify**: Both commands exit with status 0.

- [x] 3.30 Verify include-path isolation in standalone tree:
      `grep -r '#include <util/' filter-chain/src/` and
      `grep -r '#include <render/' filter-chain/src/` both return zero matches.
      **Verify**: Both grep commands return exit status 1 (no matches).

## Phase 4: Assets + Diagnostics Decoupling

### 4A: Diagnostics source migration

- [x] 4.1 Move (`git mv`) the 4 diagnostics `.cpp` files from `src/util/diagnostics/` to
      `filter-chain/src/diagnostics/`: `log_sink.cpp`, `test_harness_sink.cpp`,
      `diagnostic_session.cpp`, `gpu_timestamp_pool.cpp`.
      **Verify**: `ls filter-chain/src/diagnostics/*.cpp | wc -l` returns 4.

- [x] 4.2 Move (`git mv`) all ~18 diagnostics `.hpp` files from `src/util/diagnostics/` to
      `filter-chain/src/diagnostics/`: `binding_ledger.hpp`, `chain_manifest.hpp`,
      `compile_report.hpp`, `degradation_ledger.hpp`, `diagnostic_event.hpp`,
      `diagnostic_policy.hpp`, `diagnostic_report.hpp`, `diagnostic_session.hpp`,
      `diagnostic_sink.hpp`, `execution_timeline.hpp`, `forensic.hpp`,
      `gpu_timestamp_pool.hpp`, `log_sink.hpp`, `semantic_ledger.hpp`, `session_identity.hpp`,
      `source_provenance.hpp`, `test_harness_sink.hpp`.
      **Verify**: `ls filter-chain/src/diagnostics/*.hpp | wc -l` returns at least 17.

- [x] 4.3 Update include paths in moved diagnostics files:
      - `#include <util/error.hpp>` -> `#include <goggles/filter_chain/error.hpp>`
      - `#include <util/logging.hpp>` -> `#include "support/logging.hpp"`
      - `#include <util/profiling.hpp>` -> `#include "support/profiling.hpp"`
      - Any `#include <util/diagnostics/X.hpp>` -> `#include "diagnostics/X.hpp"`
      **Verify**: `grep -r '#include <util/' filter-chain/src/diagnostics/` returns zero matches.

- [x] 4.4 Add `fc_diagnostics_obj` OBJECT library to `filter-chain/CMakeLists.txt` with the 4
      `.cpp` files. Link PRIVATE `Vulkan::Vulkan` and `spdlog::spdlog` (for `log_sink.cpp`).
      Do NOT link `goggles_util`. Handle PIC for SHARED builds. Carry forward the
      `GOGGLES_DIAGNOSTICS_FORENSIC` option and compile definition.
      Add `fc_diagnostics_obj` to the composed `goggles-filter-chain` target objects.
      **Verify**: `cmake --build /tmp/fc-build --target fc_diagnostics_obj` succeeds.

- [x] 4.5 Remove `src/util/diagnostics/CMakeLists.txt`. Update `src/util/CMakeLists.txt` to remove
      the `add_subdirectory(diagnostics)` call. The `goggles_diagnostics` target no longer exists
      in the Goggles tree (it is now `fc_diagnostics_obj` in the standalone project).
      **Verify**: `pixi run build -p debug` succeeds. No references to `goggles_diagnostics`
      remain in Goggles CMake files.

- [x] 4.6 Verify diagnostics decoupling: the standalone `fc_diagnostics_obj` target does not link
      `goggles_util`, `toml11`, `BS_thread_pool`, or `Threads`.
      **Verify**: Inspect `filter-chain/CMakeLists.txt` -- `fc_diagnostics_obj` link libraries
      contain only `Vulkan::Vulkan` and `spdlog::spdlog`.

### 4B: Asset package creation

- [x] 4.7 Copy test shader fixtures from `shaders/retroarch/test/` to
      `filter-chain/assets/shaders/test/` (13 files: `format.slang`, `format.slangp`,
      `history.slang`, `history.slangp`, `feedback.slang`, `feedback.slangp`,
      `feedback-noncausal.slang`, `feedback-noncausal.slangp`, `frame_count.slang`,
      `frame_count.slangp`, `pragma-name.slang`, `pragma-name.slangp`, `decode-format.slang`).
      Goggles retains its originals for its own tests.
      **Verify**: `ls filter-chain/assets/shaders/test/ | wc -l` returns 13.

- [x] 4.8 Move (`git mv`) diagnostics test corpus from
      `tests/util/test_data/filter_chain_diagnostics/` to `filter-chain/assets/diagnostics/`.
      This includes `authoring_corpus/` (7 files) and `semantic_probes/` (6 files).
      **Verify**: `find filter-chain/assets/diagnostics/ -type f | wc -l` returns 13.

- [x] 4.9 Copy the curated upstream shader subset for zfast integration tests to
      `filter-chain/assets/shaders/upstream/crt/`. Identify exactly which files
      `test_zfast_integration.cpp` and `test_shader_validation.cpp` load (crt-lottes-fast.slangp
      and any `.slang` dependencies it references). Copy only those files.
      **Verify**: `ls filter-chain/assets/shaders/upstream/crt/` lists the required preset and
      shader files.

### 4C: Contract test migration

- [x] 4.10 Move (`git mv`) the following 22 contract test files from `tests/render/` to
      `filter-chain/tests/contract/`:
      - `test_filter_chain.cpp`
      - `test_filter_chain_c_api_contracts.cpp`
      - `test_filter_chain_retarget_contract.cpp`
      - `test_filter_controls.cpp`
      - `test_preset_parser.cpp`
      - `test_retroarch_preprocessor.cpp`
      - `test_slang_reflect.cpp`
      - `test_shader_runtime.cpp`
      - `test_semantic_binder.cpp`
      - `test_zfast_integration.cpp`
      - `test_shader_validation.cpp`
      - `test_runtime_diagnostics.cpp`
      - `test_diagnostic_event_model.cpp`
      - `test_diagnostic_sinks.cpp`
      - `test_binding_ledger.cpp`
      - `test_diagnostic_ledgers.cpp`
      - `test_diagnostic_session.cpp`
      - `test_diagnostic_reporting.cpp`
      - `test_source_provenance.cpp`
      - `test_compile_report.cpp`
      - `test_authoring_validation.cpp`
      - `test_gpu_timestamp_pool.cpp`
      Also move `test_shader_batch_report.cpp`, `shader_batch_report.cpp`,
      `shader_batch_report.hpp`, and `shader_batch_report_main.cpp`.
      **Verify**: `ls filter-chain/tests/contract/test_*.cpp | wc -l` returns at least 22.

- [x] 4.11 Update include paths in all moved test files:
      - `#include <util/diagnostics/X.hpp>` -> `#include "diagnostics/X.hpp"`
      - `#include <render/chain/X.hpp>` -> `#include "chain/X.hpp"`
      - `#include <render/shader/X.hpp>` -> `#include "shader/X.hpp"`
      - `#include <render/texture/X.hpp>` -> `#include "texture/X.hpp"`
      - `#include <util/error.hpp>` -> `#include <goggles/filter_chain/error.hpp>`
      - Replace `GOGGLES_SOURCE_DIR` usage with `FILTER_CHAIN_ASSET_DIR` for asset path resolution.
      **Verify**: `grep -r 'GOGGLES_SOURCE_DIR' filter-chain/tests/` returns zero matches.

- [x] 4.12 Create `filter-chain/tests/CMakeLists.txt` defining the `fc_tests` executable:
      - Source files: all moved contract test `.cpp` files under `contract/`
      - Link: `goggles-filter-chain`, `Catch2::Catch2WithMain`
      - Compile definition: `FILTER_CHAIN_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"`
      - Include dir: `${CMAKE_CURRENT_SOURCE_DIR}/../src` (for PRIVATE header access in tests)
      - Register with CTest: `add_test(NAME fc_contract_tests COMMAND fc_tests)`
      - Optionally add `fc_shader_batch_report` executable for batch report tool.
      **Verify**: `cmake -S filter-chain/ -B /tmp/fc-build && cmake --build /tmp/fc-build`
      builds the test executable.

- [x] 4.13 Update Goggles `tests/CMakeLists.txt`: remove the ~22 moved contract test source
      references from `goggles_tests`. Keep the 3 host integration tests:
      `test_filter_boundary_contracts.cpp`, `test_vulkan_backend_subsystem_contracts.cpp`,
      `test_filter_chain_retarget.cpp`. Remove the `shader_batch_report` sources and executable
      if moved to standalone. Remove the visual test runner reference if it was moved.
      **Verify**: `pixi run build -p debug` succeeds. The `goggles_tests` target compiles
      with only host integration + utility tests.

### 4D: Phase 4 build gate

- [x] 4.14 Verify standalone contract tests pass:
      `cmake -S filter-chain/ -B /tmp/fc-build && cmake --build /tmp/fc-build && ctest --test-dir /tmp/fc-build --output-on-failure`.
      Fix any test failures from asset path changes or include path migration.
      **Verify**: All contract tests pass (exit status 0).

- [x] 4.15 Verify Goggles builds and tests pass after test migration:
      `pixi run build -p asan && pixi run test -p asan`.
      **Verify**: Both commands exit with status 0.

- [x] 4.16 Verify code quality gate:
      `pixi run build -p quality`.
      Fix any clang-tidy regressions in remaining Goggles code.
      **Verify**: Exit status 0.

- [x] 4.17 Verify include-path isolation is preserved after diagnostics move:
      `grep -r '#include <util/' filter-chain/src/` returns zero matches.
      `grep -r '#include <render/' filter-chain/src/` returns zero matches.
      **Verify**: Both grep commands return exit status 1 (no matches).

## Phase 5: Install/Export + find_package Switch + Preset Cleanup

### 5A: Install and export rules

- [x] 5.1 Add CMake install rules to `filter-chain/CMakeLists.txt`:
      - `install(TARGETS goggles-filter-chain EXPORT GogglesFilterChainTargets ...)`
        with `ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}`,
        `LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}`,
        `RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}`.
      - `install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})`
        for public headers.
      - `install(DIRECTORY assets/ DESTINATION share/goggles-filter-chain/assets)`
        for the asset package.
      - `install(EXPORT GogglesFilterChainTargets NAMESPACE GogglesFilterChain::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GogglesFilterChain)`
      **Verify**: `cmake --install /tmp/fc-build --prefix /tmp/fc-install` succeeds.
      `ls /tmp/fc-install/include/goggles/filter_chain/` lists the 5 public headers.
      `ls /tmp/fc-install/lib/` lists the library artifact.

- [x] 5.2 Create `filter-chain/cmake/GogglesFilterChainConfig.cmake.in` package config template:
      - Use `@PACKAGE_INIT@` and `configure_package_config_file()`.
      - Include `FilterChainDependencies.cmake` for transitive PUBLIC dependency resolution
        (Vulkan, expected-lite only -- PRIVATE deps not forwarded).
      - Include the generated `GogglesFilterChainTargets.cmake`.
      - Define `GOGGLES_FILTER_CHAIN_ASSET_DIR` pointing to installed assets.
      **Verify**: Template file contains `@PACKAGE_INIT@` and `include()` for dependencies
      and targets.

- [x] 5.3 Add `configure_package_config_file()` and `write_basic_package_version_file()` to
      `filter-chain/CMakeLists.txt`. Install the generated config and version files to
      `${CMAKE_INSTALL_LIBDIR}/cmake/GogglesFilterChain/`. Also install
      `FilterChainDependencies.cmake` to the same directory so the config can include it.
      **Verify**: After install, `ls /tmp/fc-install/lib/cmake/GogglesFilterChain/` shows
      `GogglesFilterChainConfig.cmake`, `GogglesFilterChainConfigVersion.cmake`,
      `GogglesFilterChainTargets.cmake`, `FilterChainDependencies.cmake`.

- [x] 5.4 Build and install both STATIC and SHARED variants to separate prefixes:
      - STATIC: `cmake -S filter-chain/ -B /tmp/fc-static -DFILTER_CHAIN_LIBRARY_TYPE=STATIC &&
        cmake --build /tmp/fc-static && cmake --install /tmp/fc-static --prefix /tmp/fc-static-install`
      - SHARED: `cmake -S filter-chain/ -B /tmp/fc-shared -DFILTER_CHAIN_LIBRARY_TYPE=SHARED &&
        cmake --build /tmp/fc-shared && cmake --install /tmp/fc-shared --prefix /tmp/fc-shared-install`
      **Verify**: STATIC install produces `.a` file. SHARED install produces `.so` file.

### 5B: Consumer validation projects

- [x] 5.5 Create `filter-chain/tests/consumer/static/CMakeLists.txt` and
      `filter-chain/tests/consumer/static/main.cpp`:
      - CMakeLists: `project(FCStaticConsumer)`, `find_package(GogglesFilterChain CONFIG REQUIRED)`,
        `add_executable(static_consumer main.cpp)`,
        `target_link_libraries(static_consumer PRIVATE GogglesFilterChain::goggles-filter-chain)`
      - main.cpp: Minimal consumer that includes `<goggles_filter_chain.hpp>` and
        `<goggles/filter_chain/filter_controls.hpp>`, instantiates a type, returns 0.
      **Verify**: `cmake -S filter-chain/tests/consumer/static -B /tmp/fc-consumer-static
        -DCMAKE_PREFIX_PATH=/tmp/fc-static-install && cmake --build /tmp/fc-consumer-static`
      succeeds.

- [x] 5.6 Create `filter-chain/tests/consumer/shared/CMakeLists.txt` and
      `filter-chain/tests/consumer/shared/main.cpp`:
      - Same structure as static consumer but links shared variant.
      **Verify**: `cmake -S filter-chain/tests/consumer/shared -B /tmp/fc-consumer-shared
        -DCMAKE_PREFIX_PATH=/tmp/fc-shared-install && cmake --build /tmp/fc-consumer-shared`
      succeeds.

### 5C: Goggles find_package switch

- [x] 5.7 Add `option(GOGGLES_USE_BUNDLED_FILTER_CHAIN ...)` to the root `CMakeLists.txt` or
      `src/render/CMakeLists.txt`. When OFF (default), use
      `find_package(GogglesFilterChain CONFIG REQUIRED)`. When ON, use
      `add_subdirectory(${CMAKE_SOURCE_DIR}/filter-chain ...)`.
      Both paths make `GogglesFilterChain::goggles-filter-chain` available for
      `target_link_libraries(goggles_render ...)`.
      **Verify**: `pixi run build -p debug` succeeds with
      `-DCMAKE_PREFIX_PATH=/tmp/fc-static-install` (find_package path).

- [x] 5.8 Update CMake presets in `CMakePresets.json` to include
      `CMAKE_PREFIX_PATH` or `GOGGLES_USE_BUNDLED_FILTER_CHAIN` as needed so existing presets
      (debug, release, asan, quality, test) work with the find_package path.
      **Verify**: `pixi run build -p asan && pixi run test -p asan` passes using the installed
      filter-chain package.

- [x] 5.9 Remove the transitional `.shared` hidden preset and `test-shared` configure/build/test
      presets from `CMakePresets.json`. The standalone project now owns shared-variant validation.
      **Verify**: `grep -c 'shared' CMakePresets.json` returns 0 (or only references that are
      not preset names). `pixi run build -p asan` still succeeds.

### 5D: Phase 5 build gate

- [x] 5.10 Verify consumer validation for STATIC linkage:
      `cmake -S filter-chain/tests/consumer/static -B /tmp/consumer-s
        -DCMAKE_PREFIX_PATH=/tmp/fc-static-install &&
      cmake --build /tmp/consumer-s && /tmp/consumer-s/static_consumer`.
      **Verify**: Consumer configures, builds, and runs (exit 0).

- [x] 5.11 Verify consumer validation for SHARED linkage:
      `cmake -S filter-chain/tests/consumer/shared -B /tmp/consumer-d
        -DCMAKE_PREFIX_PATH=/tmp/fc-shared-install &&
      cmake --build /tmp/consumer-d &&
      LD_LIBRARY_PATH=/tmp/fc-shared-install/lib /tmp/consumer-d/shared_consumer`.
      **Verify**: Consumer configures, builds, and runs (exit 0).

- [x] 5.12 Verify Goggles build + test with find_package consumption:
      `pixi run build -p asan && pixi run test -p asan`.
      **Verify**: Both commands exit with status 0.

- [x] 5.13 Verify quality gate: `pixi run build -p quality`.
      **Verify**: Exit status 0.

## Phase 6: End-to-End Verification

- [x] 6.1 Clean-checkout standalone build verification: from a clean build directory
      (remove any previous `/tmp/fc-*` artifacts), configure, build, test, and install the
      standalone project:
      ```
      rm -rf /tmp/fc-e2e
      cmake -S filter-chain/ -B /tmp/fc-e2e
      cmake --build /tmp/fc-e2e
      ctest --test-dir /tmp/fc-e2e --output-on-failure
      cmake --install /tmp/fc-e2e --prefix /tmp/fc-e2e-install
      ```
      **Verify**: All 4 commands exit with status 0. All ~22 contract tests pass.

- [x] 6.2 Run Goggles full CI-parity gate against the installed package:
      `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`.
      **Verify**: All 3 commands exit with status 0.

- [x] 6.3 Verify include-path isolation audit:
      `grep -r '#include <util/' filter-chain/src/` returns zero matches.
      `grep -r '#include <render/' filter-chain/src/` returns zero matches.
      `grep -r 'GOGGLES_SOURCE_DIR' filter-chain/` returns zero matches.
      **Verify**: All 3 grep commands return exit status 1.

- [x] 6.4 Verify CMake dependency graph isolation: `cmake -S filter-chain/ -B /tmp/fc-graph
      --graphviz=/tmp/fc-graph/deps.dot` and inspect `/tmp/fc-graph/deps.dot` for any edges
      to `goggles_util`, `goggles_render`, `goggles_app`, or other Goggles targets.
      **Verify**: No Goggles target names appear in the dependency graph.

- [x] 6.5 Verify host integration tests still exercise the filter-chain boundary correctly:
      run `ctest --preset test -R "test_filter_boundary_contracts|test_vulkan_backend|test_filter_chain_retarget"
      --output-on-failure` and confirm the 3 retained host tests pass.
      **Verify**: All 3 host integration tests pass.

- [x] 6.6 Update delta specs to mark requirements as verified: review each spec under
      `openspec/changes/standalone-filter-chain-extraction/specs/*/spec.md` and confirm all
      GIVEN/WHEN/THEN scenarios have been exercised by the verification steps above.
      Document any spec gaps or deviations discovered during verification.
      **Verify**: Each spec requirement has at least one corresponding verification step
      in the task breakdown that has been executed.
