## Why

The current `split-filter-chain-library` work already delivered a useful Goggles milestone: the
post-retarget contract is explicit, host code stays on the stable wrapper boundary, boundary-owned
support headers exist, tests distinguish reusable contract coverage from Goggles-owned host coverage,
and the build can rehearse alternate library sourcing. That is real value, but it is a consumer
boundary cleanup inside Goggles, not proof that the filter-chain is finished as a standalone project.

## Problem

- The original `split-filter-chain-library` framing mixes Goggles consumer cleanup with broader
  standalone-library claims.
- The implemented code proves that Goggles can consume `goggles-filter-chain` through a stable
  public boundary plus normalized source selection, but it does not prove independent repository or
  installed-surface readiness outside the monorepo.
- Keeping both ideas under one change makes the current milestone sound more complete than the code
  actually demonstrates.

## Scope

- Preserve the post-retarget host/library contract as the baseline for Goggles consumption.
- Define consumer boundary cleanup as the Goggles-side milestone: backend and controller code
  consume `goggles-filter-chain` only through stable public headers and wrapper contracts.
- Define library source selection as the build-system seam that lets Goggles resolve the library from
  in-tree, subdirectory/local-dev, or installed-package sources while keeping the downstream target
  identity stable.
- Preserve the useful completed work from the current implementation, including support headers,
  compatibility forwarders, boundary-owned public include cleanup, target normalization, test
  ownership split, and monorepo package/install rehearsal.
- Keep the validation claim narrow: Goggles rehearses its own normalized consumer boundary without
  claiming final external asset ownership or standalone project proof.

## What Changes

- Replace the repo-split framing with a proposal focused on Goggles consumer boundary cleanup.
- Treat the current provider work as normalized library source selection inside Goggles rather than as
  evidence of finished standalone extraction.
- Freeze the Goggles-side expectation that backend and controller code stay on the stable
  `goggles-filter-chain` public surface.
- Keep the post-retarget behavior explicit: output-format changes use retarget, preserve
  source-independent preset state, and do not collapse into full preset reload.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `goggles-filter-chain`: clarify that the current milestone is a stable Goggles consumer boundary in
  the monorepo.
- `build-system`: clarify that current work establishes normalized filter-chain source selection for
  Goggles consumption while preserving the downstream target name `goggles-filter-chain`.
- `filter-chain-c-api`: preserve the public C boundary as durable host/library contract surface.
- `filter-chain-cpp-wrapper`: preserve the C++ wrapper as the backend-facing C++ integration surface.
- `render-pipeline`: preserve the post-retarget ownership split between host backend behavior and
  boundary-owned runtime behavior.

## Non-goals

- Do not claim that `goggles-filter-chain` is already ready to live in a separate repository.
- Do not require install/export independence from Goggles, Pixi, Conda, or monorepo include layout.
- Do not define final external repository packaging, fixture ownership, or public-only test strategy
  beyond what Goggles needs as a consumer.
- Do not redesign preset semantics, shader semantics, pass ordering, or runtime ownership beyond the
  already-verified post-retarget contract.
- Do not treat the current package rehearsal as proof of external downstream adoption beyond Goggles.

## Impact

- Affected modules: `src/render/backend`, `src/render/chain`, `src/render/shader`,
  `src/render/texture`, `tests/render`, and build wiring under `cmake/`, `src/render/`,
  `scripts/task/`, and preset/task configuration.
- Likely affected files: `cmake/GogglesFilterChainProvider.cmake`, `src/render/CMakeLists.txt`,
  `src/render/chain/api/c/goggles_filter_chain.h`,
  `src/render/chain/api/cpp/goggles_filter_chain.hpp`, `src/render/chain/filter_controls.hpp`,
  `src/render/chain/vulkan_context.hpp`, `src/util/error.hpp`, `tests/CMakeLists.txt`, and the
  host/boundary contract tests under `tests/render/`.
- Impacted OpenSpec specs: `openspec/specs/goggles-filter-chain/spec.md`,
  `openspec/specs/build-system/spec.md`, `openspec/specs/filter-chain-c-api/spec.md`,
  `openspec/specs/filter-chain-cpp-wrapper/spec.md`, and `openspec/specs/render-pipeline/spec.md`.
- Policy-sensitive areas: ownership/lifetime split, boundary-safe public includes, one-way target
  dependency direction, and avoiding backend dependence on concrete chain internals.

## Risks

- The renamed milestone can still be misread as partial extraction unless the artifacts stay explicit
  that standalone project proof remains out of scope.
- Goggles-side success can hide remaining install/export and fixture coupling that only appears once
  the library leaves the monorepo.
- Future work can overfit the provider seam to monorepo assumptions if the next change does not
  re-evaluate the external package contract.

## Validation Plan

Verification contract:
- Build and boundary checks:
  - `pixi run build -p debug`
  - `pixi run build -p quality`
- Contract and host coverage:
  - `ctest --preset test -R "^(goggles_unit_tests|goggles_filter_chain_contract_tests)$" --output-on-failure`
- Rehearsal checks:
  - run `pixi run rehearse-filter-chain-provider` and confirm Goggles still links against downstream
    target name `goggles-filter-chain` in the in-tree baseline and monorepo package rehearsal, while
    `subdir` preserves the same configure-time contract
- Pass criteria:
  - Goggles host/backend code consumes the filter chain only through stable public boundary headers
    and wrappers
  - build wiring preserves the normalized `goggles-filter-chain` target identity across supported
    sourcing modes
  - post-retarget behavior remains the preserved consumer contract
  - artifact text stays explicit that standalone extraction remains unfinished
