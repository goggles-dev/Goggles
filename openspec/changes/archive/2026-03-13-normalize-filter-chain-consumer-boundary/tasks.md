# Tasks: Normalize Filter Chain Consumer Boundary

## Phase 1: Move public support ownership to the filter-chain boundary

- [x] 1.1 Create and use boundary-owned support headers under
      `src/render/chain/include/goggles/filter_chain/` for `Error`, `Result`, `ScaleMode`, filter
      controls, Vulkan context, and diagnostics-facing support types.
- [x] 1.2 Keep compatibility forwarders in place at `src/render/chain/filter_controls.hpp`,
      `src/render/chain/vulkan_context.hpp`, and `src/util/error.hpp` so existing in-tree include
      paths keep building while public ownership moves to the boundary.
- [x] 1.3 Update public C and C++ filter-chain headers so they no longer expose Goggles-only
      `src/util/*` or backend-private includes as part of the consumer contract.

## Phase 2: Keep Goggles backend and controller on the stable boundary

- [x] 2.1 Update `src/render/backend/filter_chain_controller.hpp` and
      `src/render/backend/filter_chain_controller.cpp` to consume `FilterChainRuntime`,
      boundary-owned filter controls, and boundary-owned Vulkan context only.
- [x] 2.2 Preserve the post-retarget split in host code: swapchain/present lifecycle remains host
      owned, while format-only retarget uses wrapper calls that preserve active preset state.
- [x] 2.3 Add or keep source-audit coverage in `tests/render/test_filter_boundary_contracts.cpp`
      and `tests/render/test_vulkan_backend_subsystem_contracts.cpp` so backend and app/UI code do
      not regain concrete chain dependencies.

## Phase 3: Normalize library source selection inside Goggles

- [x] 3.1 Add `cmake/GogglesFilterChainProvider.cmake` to resolve `goggles-filter-chain` in
      `in-tree`, `subdir`, and `package` modes.
- [x] 3.2 Update `src/render/CMakeLists.txt` so `goggles_render` always links the normalized
      `goggles-filter-chain` target instead of constructing provider-specific consumer logic.
- [x] 3.3 Keep install/export wiring in `src/render/CMakeLists.txt` and
      `cmake/GogglesFilterChainConfig.cmake.in` aligned with the boundary-owned include surface used
      by Goggles.
- [x] 3.4 Add monorepo rehearsal presets and task wiring in `CMakePresets.json`, `pixi.toml`, and
      `scripts/task/rehearse-filter-chain-provider.sh` for the in-tree baseline and installed-package
      consumer path.

## Phase 4: Split reusable contract coverage from Goggles host coverage

- [x] 4.1 Keep reusable boundary coverage in `goggles_filter_chain_contract_tests` with sources such
      as `tests/render/test_filter_chain_c_api_contracts.cpp` and
      `tests/render/test_filter_chain_retarget_contract.cpp`.
- [x] 4.2 Keep Goggles-owned backend/controller coverage in `goggles_tests`, including
      `tests/render/test_filter_boundary_contracts.cpp` and
      `tests/render/test_vulkan_backend_subsystem_contracts.cpp`.
- [x] 4.3 Preserve package-mode test behavior where Goggles host coverage still runs against the
      normalized consumer boundary while contract coverage remains authored against the in-tree
      boundary target.

## Phase 5: Verify the implemented PR 1 boundary state

- [x] 5.1 Verify build health with `pixi run build -p debug`.
- [x] 5.2 Verify static-analysis-oriented build health with `pixi run build -p quality`.
- [x] 5.3 Verify reusable and host boundary coverage with
      `ctest --preset test -R "^(goggles_unit_tests|goggles_filter_chain_contract_tests)$" --output-on-failure`.
- [x] 5.4 Verify normalized provider rehearsal with `pixi run rehearse-filter-chain-provider`.
- [x] 5.5 Align the OpenSpec artifact set for `normalize-filter-chain-consumer-boundary` to this
      implemented PR 1 state.
