## Context

The current filter-chain subsystem already exposes stable public gates through
`src/render/chain/api/cpp/goggles_filter_chain.hpp` and `src/render/chain/api/c/goggles_filter_chain.h`,
but long-lived ownership still spans `FilterChain`, `FilterChainCore`, and
`FilterChainController`. The controller currently owns async rebuild orchestration plus filter
semantics such as stage-policy application, control forwarding, and prechain resolution shaping,
while `FilterChain` and `FilterChainCore` still mix preset loading, runtime allocations, control
state, and record-path execution.

This refactor crosses `src/render/chain`, `src/render/backend`, and render regression tests, so the
design MUST preserve the existing wrapper/runtime contract, keep render-pipeline behavior stable,
and leave the backend on boundary-owned interfaces only.

## Goals / Non-Goals

**Goals:**
- Keep the C++ wrapper and C ABI as the only stable filter-chain gates.
- Move filter semantics behind one gate-owned runtime boundary.
- Split internal responsibilities into build, resources, execution, and controls without changing
  stage ordering or gate behavior.
- Shrink `FilterChainController` to async rebuild submission, safe swap timing, and runtime
  retirement orchestration.
- Preserve active policy, control state, and prechain configuration across successful runtime swaps.
- Keep failed reloads non-destructive and keep record-path work bounded.

**Non-Goals:**
- Change shader semantic contracts, pass ordering, or public wrapper/C ABI surface shape.
- Move filter semantics back into backend code.
- Require explicit fence-backed retirement if the current backend plumbing cannot support it yet.
- Perform unrelated backend/render cleanup outside the gate-centered refactor.

## Decisions

### Decision: Make the stable gates the only supported runtime boundary

`FilterChainRuntime` in `src/render/chain/api/cpp/goggles_filter_chain.hpp` and the C ABI in
`src/render/chain/api/c/goggles_filter_chain.h` SHALL remain the only stable filter-chain
integration gates. Backend code under `src/render/backend/` SHALL continue to interact through the
wrapper-owned runtime only, while internal runtime types remain hidden behind the C ABI handle.

Rationale:
- The living wrapper spec already treats the C++ wrapper as the runtime integration boundary.
- Keeping the gate stable lets the deep refactor stay internal instead of turning into a broad
  backend migration.

Alternatives considered:
- Keep the current facade/core split and only rename classes: rejected because it preserves the same
  ownership leakage across controller, facade, and core.
- Let backend code talk to new internal runtime types directly: rejected because it breaks boundary
  isolation and widens the change surface unnecessarily.

### Decision: Preserve the backend-owned Vulkan handoff seam

`VulkanBackend` SHALL retain ownership of swapchain, external-frame import, synchronization, render
output, and root backend Vulkan state. The filter-chain runtime SHALL continue to receive only
boundary-scoped Vulkan inputs from `VulkanBackend::make_filter_chain_build_config()` and
`backend_internal::VulkanContext::boundary_context(...)`; the refactor SHALL NOT move root backend
Vulkan or render-output ownership behind `ChainRuntime`.

Rationale:
- The current backend/chain seam already isolates root Vulkan and presentation ownership from filter
  semantics.
- Preserving that seam keeps the refactor focused on filter-chain ownership instead of collapsing the
  backend/render-output split.

Alternatives considered:
- Move root backend Vulkan state into the new chain runtime: rejected because it breaks the current
  boundary handoff contract and widens the refactor into swapchain/import/present ownership.
- Let the chain runtime reach back into backend render-output state ad hoc: rejected because it
  recreates cross-module coupling through a different seam.

### Decision: Collapse facade/core ownership into one runtime-owned boundary, then split by role

The C ABI handle SHALL own one internal `ChainRuntime` object that becomes the single runtime owner
behind the stable gates. `ChainRuntime` SHALL coordinate an immutable compiled-chain product plus
runtime resources, control state, and requested policy/configuration. Internal decomposition SHALL
separate at least these responsibilities:
- `ChainBuilder` for preset parsing, include/texture resolution, shader compilation, and immutable
  compiled output creation.
- `ChainResources` for framebuffers, history, feedback, texture registry, and resize-sensitive
  allocations.
- `ChainExecutor` for hot-path `record()` execution in fixed stage order.
- `ChainControls` for stable control ids, descriptors, defaults, normalization, and rebuild replay.

Rationale:
- One runtime owner makes async install/swap semantics explicit.
- Separating immutable build output from mutable runtime state keeps hot-path and rebuild-path logic
  from drifting back together.

Alternatives considered:
- Keep `FilterChain` as the top-level owner and extract helpers under it: rejected because the facade
  would still remain a second runtime boundary behind the stable gates.
- Split everything immediately without a single runtime owner: rejected because swap/install
  semantics become harder to reason about during migration.

### Decision: Keep controller scope to orchestration only

`FilterChainController` SHALL own active/pending runtime instances, async `JobSystem` submission,
swap-at-safe-point behavior, and retired-runtime cleanup. Stage-policy interpretation, prechain
resolution normalization, and control semantics SHALL move behind `ChainRuntime` so backend code only
forwards desired values into the gate.

Rationale:
- The controller should manage runtime lifetimes, not encode filter semantics.
- Moving semantics behind the gate aligns the implementation with the wrapper boundary contract.

Alternatives considered:
- Leave control normalization or prechain math in the controller: rejected because it keeps the
  backend coupled to filter semantics and makes reload/state replay harder to centralize.

### Decision: Treat reload and record as separate contracts

Async reload SHALL build a complete pending runtime payload without mutating the active runtime. A
successful swap SHALL preserve policy, control values, and prechain configuration before the new
runtime renders its first frame. The active `record()` path SHALL consume prepared compiled state and
resources only, and SHALL NOT parse presets, compile shaders, touch the filesystem, or block on
background job completion.

Rationale:
- Failure isolation is necessary for a deep refactor that preserves runtime continuity.
- Explicit hot-path constraints prevent architectural cleanup from regressing frame-loop behavior.

Alternatives considered:
- Mutate the active runtime in place during background rebuild: rejected because it couples failure
  handling to the live frame path.
- Permit record-path rebuild work when state drifts: rejected because it hides unbounded work in the
  frame loop.

### Decision: Prefer explicit retirement tracking but keep a bounded fallback

The refactor SHALL isolate swapped-out runtime retirement into a helper with explicit safety rules.
If current backend plumbing supports submission-epoch or fence-backed retirement, the implementation
SHALL prefer that mechanism. Otherwise it SHALL keep the existing delayed-retire strategy as an
explicit fallback with dedicated verification coverage.

Rationale:
- Runtime destruction safety is part of the swap contract, not incidental cleanup.
- An isolated fallback keeps the heuristic bounded instead of scattering it through controller logic.

Alternatives considered:
- Keep the frame-delay heuristic inlined in controller code forever: rejected because it hides safety
  assumptions in orchestration logic.
- Block the refactor on immediate fence plumbing: rejected because the architecture improvement can
  still proceed with a bounded fallback path.

## Risks / Trade-offs

- [Ownership drift during migration] -> Introduce `ChainRuntime` first so later splits still route
  through one owner.
- [Controller still knows too much] -> Move policy/prechain/control interpretation behind the gate
  before treating the refactor as complete.
- [Record-path regressions] -> Keep explicit tests and contract language around stage ordering,
  record-path boundedness, and control stability.
- [Retirement safety remains heuristic] -> Isolate the fallback, keep it bounded, and verify it with
  dedicated boundary-contract coverage.

## Migration Plan

1. Introduce `ChainRuntime` and route the C ABI handle plus C++ wrapper through it while keeping the
   public gate surface unchanged.
2. Move current long-lived state from `FilterChain` / `FilterChainCore` into `ChainRuntime` and keep
   `FilterChain` only as a temporary adapter if needed during migration.
3. Extract immutable build output into `ChainBuilder` / `CompiledChain` and separate mutable runtime
   allocations into `ChainResources`.
4. Extract hot-path recording into `ChainExecutor` and control/state replay into `ChainControls`.
5. Reduce `FilterChainController` to orchestration-only scope and isolate runtime retirement logic.
6. Remove the obsolete facade/core split once the wrapper, controller, and tests all target the new
   boundary.

Rollback strategy:
- Revert the change as one refactor unit if ownership or swap safety regresses; do not leave a
  partially split runtime where both the old facade/core and the new boundary remain authoritative.

## Open Questions

- None. The only allowed implementation-time branch is whether runtime retirement can move to
  submission/fence tracking immediately or must use the explicit bounded fallback first.
