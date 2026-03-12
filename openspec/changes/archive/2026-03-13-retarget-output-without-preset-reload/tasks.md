## 1. Runtime retarget seam

- [x] 1.1 Introduce a dedicated output-state helper in `src/render/chain/chain_resources.hpp` and
  `src/render/chain/chain_resources.cpp` so swapchain-bound output/postchain state is isolated from
  persistent preset-derived runtime state without starting from a broad `ChainResources` split.
- [x] 1.2 Add a runtime retarget operation in `src/render/chain/chain_runtime.hpp` and
  `src/render/chain/chain_runtime.cpp` that rebuilds only output-side state.
- [x] 1.3 Extend the C and C++ boundary APIs in `src/render/chain/api/c/goggles_filter_chain.h`,
  `src/render/chain/api/c/goggles_filter_chain.cpp`,
  `src/render/chain/api/cpp/goggles_filter_chain.hpp`, and
  `src/render/chain/api/cpp/goggles_filter_chain.cpp` with an output-retarget entrypoint.

## 2. Controller and backend wiring

- [x] 2.1 Update `src/render/backend/filter_chain_controller.hpp` and
  `src/render/backend/filter_chain_controller.cpp` to track authoritative output-target state and
  orchestrate retarget separately from full preset reload.
- [x] 2.2 Update `src/render/backend/vulkan_backend.hpp` and
  `src/render/backend/vulkan_backend.cpp` so format-only swapchain recreation calls the retarget
  path instead of `init_filter_chain()` when an active chain exists.
- [x] 2.3 Audit `src/app/application.cpp` trigger flow to keep format-change detection backend-owned
  and avoid reintroducing direct runtime coupling.

## 3. Preservation and failure semantics

- [x] 3.1 Preserve active preset selection, stage policy, prechain configuration, control layout,
  and parameter overrides across successful retarget.
- [x] 3.2 Keep retarget failure non-destructive: previous runtime/output state remains active and
  observable on failure.
- [x] 3.3 Define and implement retarget-vs-pending-reload interaction rules so async reload and
  format retarget do not drift into overlapping state machines, with pending runtimes pre-retargeted
  before swap whenever the authoritative output target changes during reload.

## 4. Verification

- [x] 4.1 Extend `tests/render/test_filter_chain_c_api_contracts.cpp` for retarget contract
  coverage.
- [x] 4.2 Add focused runtime/controller coverage in
  `tests/render/test_filter_chain_retarget.cpp` for preserved state, no preset rebuild semantics,
  and failure rollback.
- [x] 4.3 Update `tests/render/test_vulkan_backend_subsystem_contracts.cpp` and any relevant
  boundary tests so format-only changes expect retarget behavior while explicit reload remains the
  only full rebuild entry.
- [x] 4.4 Run `pixi run build -p test` and `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`.
