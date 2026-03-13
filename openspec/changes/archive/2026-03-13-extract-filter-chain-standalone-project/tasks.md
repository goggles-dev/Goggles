# Tasks: Extract Filter Chain Standalone Project

## Phase 1: Keep only extraction groundwork in the monorepo

- [x] 1.1 Remove smoke-matrix presets, tasks, and other package-rehearsal wiring that only served
      fake intermediate packaging modes.
      **Note:** The root `.shared` hidden preset and `test-shared` configure/build/test presets in
      `CMakePresets.json` are transitional host-integration coverage only. They verify that Goggles
      still builds and tests when the in-repo `goggles-filter-chain` target is built as SHARED.
      These presets must be removed once the standalone project owns real static/shared package
      validation (Phase 5). Do not add additional root shared-variant presets.
- [x] 1.2 Establish in-repo public include tree (`src/render/chain/include/goggles/filter_chain/`)
      mirroring the standalone project's future public surface. Migrate internal headers
      (`error.hpp`, `result.hpp`, `vulkan_context.hpp`, `filter_controls.hpp`, `scale_mode.hpp`,
      diagnostics shims) into this tree and rewrite consumers to use canonical include paths.
      **Status:** All 5 public headers are self-contained. `filter_controls.hpp` and
      `vulkan_context.hpp` define types inline. `error.hpp`, `result.hpp`, and `scale_mode.hpp`
      define types with shared `#ifndef` guards matching `util/` counterparts for ODR safety.
      The 8 diagnostics forwarding shims were removed; chain sources include
      `<util/diagnostics/...>` directly.
- [x] 1.3 Decouple `goggles-filter-chain` library target from `goggles_util` public linkage.
      Extract `goggles_util_logging_obj` OBJECT library, add `spdlog::spdlog` as direct private
      dependency, move `Vulkan::Vulkan` to PUBLIC, tighten OBJECT library visibility
      (PUBLIC -> PRIVATE) in chain, shader, and texture CMakeLists, and add
      `GogglesFilterChain::goggles-filter-chain` ALIAS for future `find_package()` consumption.
- [x] 1.4 Add `FilterChainController::record()` encapsulation so backend code delegates recording
      through the controller facade instead of reaching through to the raw runtime.
- [x] 1.5 Add retarget-preserves-state contract test exercising the C++ wrapper API with a live
      Vulkan runtime, verifying that `retarget_output()` preserves stage policy, prechain
      resolution, and control values.
- [x] 1.6 Improve C API header documentation: add `@note` annotations about host-owned handles,
      retarget semantics, and swapchain responsibilities.

## Phase 2: Finish boundary-owned public support in the current tree

- [x] 2.1 Replace remaining public-header dependence on Goggles-private support and diagnostics
      headers with library-owned headers under the future standalone `include/` surface.
      **Done:** Public headers `error.hpp`, `result.hpp`, and `scale_mode.hpp` under
      `goggles/filter_chain/` are now self-contained with inline type definitions. Shared
      `#ifndef` guards with `util/error.hpp` and `util/scale_mode.hpp` prevent ODR violations
      in the monorepo. `${CMAKE_SOURCE_DIR}/src` moved from PUBLIC to PRIVATE on
      `goggles-filter-chain` so downstream consumers no longer see the full repo source tree.
      The C API implementation now uses canonical `<goggles/filter_chain/...>` include paths.
      Boundary contract tests validate all 5 public headers are util-free.
- [x] 2.2 Move the remaining reusable runtime support now living under Goggles `src/util/` into a
      library-owned layout that can be copied to a standalone `src/` tree without host coupling.
      **Note:** Phase 2.1 made public headers self-contained via shared include guards. The
      shared-guard approach is transitional — it keeps both copies of the types in sync via
      identical definitions. The extraction phase will remove the util copies entirely.
      **Done:** Internal headers `vulkan_result.hpp`, `shader/slang_reflect.hpp`, and
      `texture/texture_loader.hpp` now include the library-owned `<goggles/filter_chain/error.hpp>`
      instead of `<util/error.hpp>`. No `.hpp` file in the chain/shader/texture subtrees includes
      non-diagnostics `util/` headers. Remaining `util/logging.hpp`, `util/profiling.hpp`, and
      `util/serializer.hpp` includes from `.cpp` implementation files are provided by the PRIVATE
      `goggles_util` link and will transfer to standalone ownership in Phase 3.
- [x] 2.3 Trim or convert any remaining compatibility forwarders that are no longer needed once the
      boundary-owned headers fully cover the public contract.
      **Done:** Removed the transitional forwarder headers `src/render/chain/vulkan_context.hpp` and
      `src/render/chain/filter_controls.hpp`. All internal chain consumers, test files, and host UI
      now include through canonical `<goggles/filter_chain/...>` paths. The `goggles_ui` target was
      given a direct `goggles-filter-chain` link to provide the public include directory. Boundary
      contract tests updated to remove forwarder-existence assertions.

## Phase 3: Create the standalone repository skeleton

- [ ] 3.1 Create the standalone project root with `CMakeLists.txt`, `cmake/`, `include/`, `src/`,
      `tests/`, and `assets/` as the owned layout.
- [ ] 3.2 Move the reusable implementation from `src/render/chain/`, `src/render/shader/`, and
      `src/render/texture/` into standalone-owned modules while preserving the current host/library
      responsibility split.
- [ ] 3.3 Move reusable contract tests into standalone-owned `tests/contract/` and keep Goggles
      tests focused on host integration only.

## Phase 4: Move assets and diagnostics under library ownership

- [ ] 4.1 Create the library-owned asset package under `assets/` for presets, shaders, fixtures,
      and related verification data required by runtime and tests.
- [ ] 4.2 Make runtime asset lookup and contract tests resolve library-owned assets without Goggles
      checkout-relative assumptions.
- [ ] 4.3 Move diagnostics support required by the public surface under standalone-owned include/src
      paths instead of Goggles utility ownership.
      **Note:** The 8 diagnostics forwarding shims (`goggles/filter_chain/diagnostics/*.hpp`) were
      removed in Phase 1 cleanup as they added no value. Chain sources already include
      `<util/diagnostics/...>` directly. This task now focuses on moving the diagnostics
      implementation itself under standalone ownership.

## Phase 5: Build, export, and consume the real package

- [ ] 5.1 Publish supported `STATIC` and `SHARED` library targets from the standalone project and do
      not define a supported `MODULE` variant.
- [ ] 5.2 Install headers, libraries, assets, and CMake package metadata, then export
      `GogglesFilterChain::goggles-filter-chain` plus explicit static/shared targets.
      **Note:** Install rules were intentionally removed from the monorepo chain target during
      Phase 1 cleanup. This task implements proper install/export from the standalone project
      with config-file packages and namespaced export sets.
- [ ] 5.3 Add downstream consumer validation that uses `find_package(GogglesFilterChain CONFIG
      REQUIRED)` for both static and shared linkage.
- [ ] 5.4 Update Goggles so installed package consumption through `find_package(...)` is the normal
      dependency path, with any source-based path kept only as an explicit development option.
- [ ] 5.5 Remove the transitional root `.shared` and `test-shared` presets from Goggles
      `CMakePresets.json`. Once the standalone project owns static/shared package validation,
      Goggles no longer needs its own shared-variant build presets. If Goggles still needs a
      host-integration shared-linkage check, it should consume the installed shared package
      through `find_package(...)`, not rebuild the library in-tree as SHARED.

## Phase 6: End-to-end verification

- [ ] 6.1 Prove the standalone project can configure, build, test, install, and run consumer
      validation from a clean checkout.
- [ ] 6.2 Run Goggles build plus focused host-side verification against the installed package to
      confirm preserved post-retarget behavior and external package consumption.
