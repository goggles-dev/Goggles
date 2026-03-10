## Why

The current filter-chain implementation still splits long-lived ownership and behavior across
`FilterChain`, `FilterChainCore`, and `FilterChainController`, so the public C++/C gates are not the
real runtime boundary yet. That makes async reload, control ownership, prechain resolution, and
stage-policy behavior harder to reason about than the stable gate contracts imply.

This change is needed now because the filter-chain C++ wrapper and render-pipeline living specs
already depend on stable gate behavior, atomic policy application, and safe async swaps. A deeper
refactor should tighten the architecture around those guarantees before more runtime features stack
onto the current facade/core split.

## Problem

- `src/render/backend/filter_chain_controller.cpp` still owns filter semantics such as stage-policy
  interpretation, prechain resolution math, and control forwarding instead of only runtime
  orchestration.
- `src/render/chain/filter_chain.hpp` and `src/render/chain/filter_chain_core.hpp` split ownership of
  preset loading, record-path behavior, runtime allocations, and control state across overlapping
  abstractions.
- The stable gate intent in `src/render/chain/api/cpp/goggles_filter_chain.hpp` and
  `src/render/chain/api/c/goggles_filter_chain.h` is harder to preserve when internal runtime state
  still leaks responsibility boundaries into backend code.
- Async reload safety and deferred destruction are present today, but the ownership model makes it
  difficult to isolate failure handling, state replay, and retirement improvements cleanly.

## Scope

- Make the C++ wrapper and C ABI the only stable filter-chain gates while keeping them
  source-compatible.
- Replace the current facade/core split with one gate-owned runtime boundary that can be decomposed
  internally by responsibility.
- Reduce `FilterChainController` to async build submission, pending swap, and runtime retirement
  orchestration.
- Preserve existing render-pipeline behavior for stage ordering, policy continuity, postchain
  presentation, and reload safety while moving ownership behind the gate.
- Add contract coverage for boundary isolation, bounded record-path behavior, and runtime swap /
  retirement guarantees.

## What Changes

- Introduce a dedicated filter-chain runtime-boundary capability that defines gate-owned runtime
  ownership, async rebuild isolation, bounded record-path behavior, and controller-scope limits.
- Strengthen the `filter-chain-cpp-wrapper` capability so the C++ wrapper and C ABI remain the sole
  stable filter-chain integration gates while internal runtime ownership changes behind them.
- Refactor the render-chain implementation so builder, executor, resources, and controls can evolve
  behind one runtime-owned boundary without changing backend-facing contracts.
- Keep existing render-pipeline stage-order and policy-preservation guarantees intact while the
  internals move behind the stable gates.

## Capabilities

### New Capabilities
- `filter-chain-runtime-boundary`: defines the gate-owned filter runtime, async swap guarantees,
  controller scope, and bounded record-path responsibilities required for the deep refactor.

### Modified Capabilities
- `filter-chain-cpp-wrapper`: the wrapper requirements change to keep the C++ wrapper and C ABI as
  the only stable filter-chain gates while the internal runtime implementation changes behind them.

## Non-goals

- Change shader semantic names, pass ordering, or postchain presentation behavior.
- Bypass the stable C++ or C gates from `src/render/backend/`.
- Lock the current proposed internal class names if a safer equivalent ownership split is found
  during implementation.
- Deliver unrelated render/backend cleanup outside the runtime-boundary refactor.

## Impact

- Affected modules: `src/render/chain`, `src/render/backend`, and `tests/render`.
- Likely affected files: `src/render/chain/filter_chain.hpp`, `src/render/chain/filter_chain.cpp`,
  `src/render/chain/filter_chain_core.hpp`, `src/render/chain/filter_chain_core.cpp`,
  `src/render/chain/api/c/goggles_filter_chain.cpp`,
  `src/render/chain/api/cpp/goggles_filter_chain.cpp`,
  `src/render/backend/filter_chain_controller.hpp`,
  `src/render/backend/filter_chain_controller.cpp`, `src/render/chain/CMakeLists.txt`,
  `tests/render/test_filter_boundary_contracts.cpp`, `tests/render/test_filter_chain.cpp`, and
  `tests/render/test_filter_controls.cpp`.
- Impacted OpenSpec specs: `openspec/specs/filter-chain-cpp-wrapper/spec.md` and the new
  `filter-chain-runtime-boundary` capability.
- No new external dependencies or packaging changes are expected.

## Risks

- Runtime ownership can still leak back into backend code if controller responsibilities are not
  narrowed decisively.
- Async reload and shutdown behavior can regress if pending runtime state is not isolated from the
  active runtime throughout failure and swap paths.
- A deep refactor can accidentally alter hot-path behavior unless record-path obligations stay
  explicit and testable.
- Retirement safety can remain heuristic if explicit submission/fence tracking is not feasible in the
  current backend plumbing.

## Validation Plan

Verification contract:
- Baseline gates:
  - `pixi run build -p debug`
  - `pixi run build -p asan`
  - `pixi run build -p quality`
- Environment-agnostic automated checks:
  - `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`
  - `pixi run test -p asan`
- Environment-sensitive checks:
  - `ctest --preset test -R "^(headless_smoke|goggles_headless_integration)$" --output-on-failure`
    when the local runtime supports the required GPU/display path
- Manual fallback:
  - allowed only for the environment-sensitive checks above
  - record prerequisites, observations, and proof location if fallback is used
- Mandatory checks with no fallback:
  - the baseline build/static-analysis gates above
  - the targeted render/filter unit coverage in `goggles_unit_tests`
- Pass criteria:
  - backend-facing code continues to use the stable filter-chain gates only
  - failed async reload leaves the active runtime usable
  - successful swaps preserve policy, controls, and prechain configuration before first render
  - record-path behavior preserves `prechain -> effect -> postchain` ordering without preset parse,
    shader compilation, filesystem I/O, or blocking waits
