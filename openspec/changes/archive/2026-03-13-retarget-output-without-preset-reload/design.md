## Context

The current render path already separates most preset-derived work from final presentation, but the
backend/controller lifecycle collapses source-format retargeting into full runtime recreation.
`VulkanBackend::recreate_swapchain()` and `FilterChainController::recreate_filter_chain()` currently
route format-only changes through the same path used for explicit preset reload behavior.

This design keeps eager preset processing intact and narrows the change to the real source-format
dependency: swapchain-bound output/postchain state.

## Goals / Non-Goals

**Goals:**
- Preserve eager preset processing.
- Separate swapchain/output retargeting from full preset reload.
- Keep source-independent preset runtime state alive across output-format retargeting.
- Preserve explicit preset reload as the only full rebuild path.
- Preserve backend ownership of swapchain lifecycle and presentation.
- Preserve control layout, parameter overrides, stage policy, and active preset selection across
  successful output retarget.

**Non-Goals:**
- Delay whole preset processing until the first source image arrives.
- Rework shader semantics, pass ordering, or unrelated backend architecture.
- Collapse backend-owned render-output responsibilities into the chain runtime.
- Turn format retarget into a hidden full preset rebuild fallback.

## Decisions

### Decision: Split persistent preset state from retargetable output state

`ChainRuntime` SHALL retain source-independent preset-derived state across output-format retargets.
Swapchain-bound state SHALL be isolated so it can be rebuilt independently.

Persistent state includes:
- parsed/installed preset state
- effect passes and effect-stage resources
- preset texture registry
- control descriptors and control values
- stage policy and prechain configuration
- diagnostics session and other runtime-owned non-output state

Retargetable state includes:
- active swapchain/output format binding
- output/postchain pass objects
- swapchain-bound postchain framebuffers and related present-path state

Rationale:
- Most preset work does not depend on the final output format.
- The duplicated startup/rebuild work exists because lifecycle code reloads the full runtime instead
  of retargeting only the output side.

### Decision: Introduce a dedicated output-state helper before deeper resource splitting

The first implementation step SHALL isolate retargetable output/postchain state behind a dedicated
helper owned by the runtime/resources layer. Splitting `ChainResources` into broader persistent vs.
retargetable sub-objects SHALL remain a follow-on refactor only if the helper boundary proves too
tight.

Rationale:
- This gives the change a narrow seam for output-format retargeting without forcing an immediate
  large-scale internal re-layout.
- It preserves current ownership and pass-graph wiring while making the swapchain-bound state
  explicit.

### Decision: Backend remains authoritative for swapchain lifecycle

`VulkanBackend` SHALL continue to own swapchain recreation and final presentation lifecycle.
Format-only source color-space changes SHALL recreate the swapchain, then call a narrower chain
retarget API instead of a full runtime rebuild path.

Rationale:
- The change is about separating retargeting from preset reload, not about moving presentation
  ownership into the chain layer.

### Decision: Explicit preset reload remains a full rebuild path

User-requested preset changes or explicit reloads SHALL continue to use the existing full preset
reload behavior through controller async rebuild orchestration.

Rationale:
- This keeps the meaning of preset reload stable.
- It prevents output-only retargeting from becoming a leaky general rebuild path.

### Decision: Overlapping format change retargets pending runtime before swap

If a format change lands while an explicit async reload already has a pending runtime, the
controller SHALL update the authoritative output target and retarget that pending runtime before it
becomes active. The controller SHALL NOT swap in a runtime built for the stale target and then
retarget it immediately afterward.

Rationale:
- Swap activation should make the new runtime current in its final output configuration.
- Pre-retargeting keeps swap-complete signaling aligned with the actually active output target and
  avoids an unnecessary extra retarget on the newly swapped runtime.

### Decision: Retarget failure preserves active runtime state

Retarget SHALL be staged as candidate output-state rebuild followed by swap-on-success.
If retarget fails, the previously active runtime state SHALL remain active and observable.

Rationale:
- Failure isolation must match the current non-destructive reload contract.

## State Model

- `active runtime`: currently rendering preset/runtime state
- `pending runtime`: full rebuilt runtime for explicit preset reloads only
- `active output state`: swapchain-bound output/postchain state associated with the active runtime
- `authoritative output target`: controller/backend view of target format and extent

Retarget path:
1. backend detects source color-space change
2. backend recreates swapchain
3. controller forwards new output target to active runtime
4. runtime builds candidate output state only
5. runtime swaps output state on success
6. previous output state is destroyed after successful replacement

Full reload path:
1. app/backend requests preset reload
2. controller builds pending runtime asynchronously
3. controller reapplies authoritative state, including the latest output target
4. if output target changed while reload was pending, controller pre-retargets pending runtime
5. pending runtime swaps in at safe point

## Module / File Plan

- `src/render/chain/chain_runtime.hpp`
- `src/render/chain/chain_runtime.cpp`
- `src/render/chain/chain_resources.hpp`
- `src/render/chain/chain_resources.cpp`
- `src/render/chain/output_pass.hpp`
- `src/render/chain/output_pass.cpp`
- `src/render/chain/api/c/goggles_filter_chain.h`
- `src/render/chain/api/c/goggles_filter_chain.cpp`
- `src/render/chain/api/cpp/goggles_filter_chain.hpp`
- `src/render/chain/api/cpp/goggles_filter_chain.cpp`
- `src/render/backend/filter_chain_controller.hpp`
- `src/render/backend/filter_chain_controller.cpp`
- `src/render/backend/vulkan_backend.hpp`
- `src/render/backend/vulkan_backend.cpp`

## Verification Strategy

- Add boundary/API coverage for the retarget operation and its validation behavior.
- Add controller/runtime tests covering:
  - format-only retarget does not trigger preset rebuild semantics
  - active preset selection and control layout remain unchanged across retarget
  - parameter overrides and policy remain intact across retarget
  - failure leaves previous runtime active
- Update backend seam tests so format change routes through retarget, while explicit reload remains
  the only full rebuild path.

## Risks / Trade-offs

- Hidden coupling between effect resources and output-side resources can complicate the split.
- Retarget and async reload sequencing must be explicit to avoid stale output-target state.
- Retarget after swapchain recreation cannot cheaply roll back swapchain ownership, so error
  reporting and runtime-state preservation both matter.

## Resolved Follow-Ups

- Use a dedicated output-state helper as the first isolation step; do not start by splitting
  `ChainResources` internally.
- When async reload and format retarget overlap, pre-retarget the pending runtime before swap.
