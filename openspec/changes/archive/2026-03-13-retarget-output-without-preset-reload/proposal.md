## Why

Goggles currently treats a source color-space change as a reason to rebuild the full filter runtime,
even though most preset work is independent of the final swapchain output format. In the common
startup path, this means Goggles can eagerly load a preset, then recreate the swapchain after the
first real source frame arrives, and reload the same preset again.

This creates duplicated parse/compile/load work, repeated failure logs for the same bad preset, and
extra churn in a path that should mostly be an output retarget.

## Problem

- `src/render/backend/vulkan_backend.cpp` treats source-format-driven swapchain recreation as a full
  filter-chain reinit path.
- `src/render/backend/filter_chain_controller.cpp` reloads the active preset again during runtime
  recreation even when the preset itself did not change.
- Most preset work is source-format-independent: preset parse, include expansion, shader
  compile/reflection, preset texture loading, and effect-pass setup.
- The truly source-format-dependent work is mainly swapchain/output-format selection and
  swapchain-bound output/postchain state.

## Scope

- Separate swapchain-format retargeting from full preset reload.
- Preserve eager preset processing and keep source-independent preset-derived runtime state alive
  across output-format retargeting.
- Rebuild only swapchain-bound output/postchain state when source color-space classification changes
  and maps to a different output format.
- Keep explicit preset reload as the only full preset/runtime rebuild path.
- Preserve existing async reload, error propagation, and backend ownership boundaries.

## What Changes

- Introduce a boundary-facing retarget operation for format-only output changes.
- Isolate retargetable output-side state behind a dedicated output-state helper before attempting any
  broader `ChainResources` split.
- Update backend/controller sequencing so swapchain recreation on format-only changes retargets the
  active runtime instead of destroying it and reloading the preset.
- Preserve the current explicit reload path for user-requested preset changes or reloads.
- When format retarget overlaps an explicit async reload, retarget the pending runtime before swap
  instead of swapping in stale output state and retargeting afterward.

## Capabilities

### Modified Capabilities
- `render-pipeline`
- `goggles-filter-chain`

## Non-goals

- Delay whole preset processing until the first real frame.
- Change shader semantic contracts, stage ordering, or unrelated filter/backend ownership.
- Redesign preset authoring or diagnostics behavior beyond the retarget-vs-reload distinction.
- Guarantee zero first-frame cost; the goal is to remove duplicated source-independent work.

## Impact

- Affected modules: `src/render/backend`, `src/render/chain`, `tests/render`.
- Likely affected files: `src/render/backend/vulkan_backend.cpp`,
  `src/render/backend/filter_chain_controller.cpp`, `src/render/backend/filter_chain_controller.hpp`,
  `src/render/chain/chain_runtime.cpp`, `src/render/chain/chain_runtime.hpp`,
  `src/render/chain/chain_resources.cpp`, `src/render/chain/chain_resources.hpp`,
  `src/render/chain/output_pass.cpp`, `src/render/chain/output_pass.hpp`,
  `src/render/chain/api/c/goggles_filter_chain.h`,
  `src/render/chain/api/c/goggles_filter_chain.cpp`,
  `src/render/chain/api/cpp/goggles_filter_chain.hpp`,
  `src/render/chain/api/cpp/goggles_filter_chain.cpp`, and targeted `tests/render/*` coverage.
- Impacted OpenSpec specs: `openspec/specs/render-pipeline/spec.md` and
  `openspec/specs/goggles-filter-chain/spec.md`.

## Risks

- Splitting persistent and retargetable state can expose hidden coupling inside `ChainResources`.
- Retarget and async reload can race unless controller state clearly distinguishes full rebuild from
  output-only retarget.
- Failure handling must keep the current active runtime usable when retargeting fails.
- Tests may currently assume format change implies full rebuild and will need contract updates.

## Resolved Implementation Direction

- Start with a dedicated output-state helper as the first isolation seam.
- On format-change / async-reload overlap, pre-retarget the pending runtime before it becomes active.

## Validation Plan

Verification contract:
- Baseline gates:
  - `pixi run build -p debug`
  - `pixi run build -p asan`
  - `pixi run build -p quality`
- Environment-agnostic automated checks:
  - `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`
- Targeted contract coverage:
  - render/filter tests for retarget-vs-reload behavior, preserved control state, preserved preset
    selection, and failure rollback behavior
- Pass criteria:
  - format-only swapchain changes do not repeat preset parse/compile/texture-load work
  - explicit preset reload remains the only full rebuild path
  - successful retarget preserves active preset state and control behavior
  - failed retarget leaves the previous active runtime usable
