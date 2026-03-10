## 1. Gate-owned runtime boundary

- [x] 1.1 Add `ChainRuntime` (and any supporting compiled-state type) behind the filter-chain C ABI
  handle, and update `src/render/chain/CMakeLists.txt` so the render-chain target builds the new
  runtime entrypoint without changing the public C/C++ gate surface.
- [x] 1.2 Update `src/render/chain/api/c/goggles_filter_chain.cpp` and
  `src/render/chain/api/cpp/goggles_filter_chain.cpp` so create/load/resize/record/control flows
  forward through the gate-owned runtime while keeping C ABI and wrapper behavior source-compatible.
- [x] 1.3 Remove backend dependence on facade/core internals by keeping `src/render/backend/` on
  `FilterChainRuntime` only and preserving boundary isolation coverage in
  `tests/render/test_filter_boundary_contracts.cpp`.
- [x] 1.4 Preserve the backend-owned Vulkan handoff seam so `VulkanBackend` keeps swapchain,
  importer, synchronization, render-output, and root `VulkanContext` ownership while passing only
  boundary-scoped Vulkan inputs into the stable filter-chain gate.

## 2. Internal runtime responsibility split

- [x] 2.1 Extract preset parse/resolve/compile/rebuild work from the current facade/core path into
  `ChainBuilder` and an immutable compiled-chain product.
- [x] 2.2 Extract runtime allocations and resize-sensitive state into `ChainResources`, including
  framebuffers, history, feedback targets, texture registry, and prechain/postchain targets.
- [x] 2.3 Extract per-frame execution into `ChainExecutor` so the record path preserves
  `prechain -> effect -> postchain` ordering without preset parsing, shader compilation,
  filesystem I/O, or blocking waits.
- [x] 2.4 Extract control catalog, normalization, defaults, deterministic ids, and rebuild replay into
  `ChainControls` so control semantics remain behind the gate.

## 3. Controller orchestration and swap safety

- [x] 3.1 Reduce `FilterChainController` to active/pending runtime orchestration, async `JobSystem`
  submission, swap-at-safe-point behavior, and retired-runtime tracking.
- [x] 3.2 Move stage-policy interpretation, prechain-resolution resolution, and control semantics
  behind `ChainRuntime` while preserving active policy, prechain configuration, and control values
  across successful swaps and keeping failed reloads non-destructive.
- [x] 3.3 Isolate runtime retirement into a helper that prefers explicit submission/fence-backed
  retirement when available and otherwise keeps the delayed-retire fallback explicitly bounded and
  tested.

## 4. Verification and contract alignment

- [x] 4.1 Update or add render tests so wrapper boundary isolation, async swap safety, stage-order
  invariants, and control-id/control-replay semantics stay aligned with the OpenSpec contract.
- [x] 4.2 Run `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` and address any
  filter-chain, control, or boundary regressions.
- [x] 4.3 Run `pixi run build -p debug`, `pixi run build -p asan`, `pixi run test -p asan`, and
  `pixi run build -p quality`.
- [x] 4.4 Run `ctest --preset test -R "^(headless_smoke|goggles_headless_integration)$" --output-on-failure`
  when the local runtime supports it; otherwise record the explicit environment limitation and the
  proof for the allowed fallback.
