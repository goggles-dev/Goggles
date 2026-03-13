## Why

Goggles already treats the filter chain as a boundary-owned dependency with explicit post-retarget
behavior. The remaining work is to extract the library into its own project with its own source
layout, support code, assets, build, install, export, and verification surface.

## Problem

- The filter-chain code still depends on Goggles repository layout, Goggles-owned support modules,
  and Goggles-owned test assets.
- The current package boundary is not yet proven as an independent install/export contract owned by
  the library project itself.
- Goggles cannot yet treat the filter chain as a normal external package dependency with
  `find_package(...)` as the expected integration path.

## Scope

- Extract `goggles-filter-chain` into a standalone CMake project rooted at `include/`, `src/`,
  `tests/`, `assets/`, and `cmake/`.
- Move required support code, diagnostics support, fixtures, and assets under library ownership.
- Ship an installable and exportable package that supports `STATIC` and `SHARED` outputs without a
  `MODULE` variant.
- Verify the installed public surface directly and make Goggles consume the library as an external
  dependency through `find_package(...)` first.
- Preserve the current post-retarget public contract.

## What Changes

- Define the extracted project as the owner of the public headers, private support modules,
  diagnostics support, test fixtures, and packaged assets required to build and verify the library
  outside Goggles.
- Require a standalone build/install/export flow that works from the extracted project root without
  Goggles wrappers or Goggles source-tree assumptions.
- Require installed-surface verification and downstream consumer checks against the exported package.
- Require Goggles to resolve the library as an external package dependency through
  `find_package(...)`, with `add_subdirectory(...)` kept only as an explicit local-development
  option.

## Capabilities

### New Capabilities
- `filter-chain-standalone-project`: defines the independent repository layout, support ownership,
  build/install/export contract, installed-surface verification, and downstream package consumption
  for the extracted filter-chain project.
- `filter-chain-assets-package`: defines the library-owned asset package used by installed contract
  verification and downstream consumers.

### Modified Capabilities
- `goggles-filter-chain`: tighten the target contract from a reusable in-repo target to a true
  standalone project package with paired `STATIC` and `SHARED` outputs.
- `build-system`: require an independent CMake build/install/export workflow plus
  `find_package(...)`-first Goggles consumption.
- `filter-chain-c-api`: require the installed C ABI surface to remain consumable outside the Goggles
  repository.
- `filter-chain-cpp-wrapper`: require the installed C++ wrapper surface to remain consumable outside
  the Goggles repository.

## Non-goals

- Do not revisit or extend the already-established Goggles consumer boundary.
- Do not use Goggles-only builds or tests as the acceptance proof for extraction.
- Do not change the post-retarget public contract.
- Do not introduce or require a `MODULE` library variant.

## Exact Success Criteria

> **Note:** These criteria describe the fully extracted end state. The current monorepo change
> implements extraction groundwork (Phase 1–2 in `tasks.md`). Criteria below will be demonstrated
> incrementally as later phases complete.

- [ ] The extracted project builds from its own repository root with layout rooted at `include/`,
      `src/`, `tests/`, `assets/`, and `cmake/`, without requiring the Goggles repository.
- [ ] The extracted project installs and exports a consumable CMake package without Goggles-specific,
      Pixi-specific, or Conda-specific assumptions.
- [ ] The extracted project publishes supported `STATIC` and `SHARED` outputs and does not require
      or expose a `MODULE` target.
- [ ] Public headers and library-owned internal code do not depend on Goggles-private `src/util/*`
      headers or Goggles source-tree include layout.
- [ ] Required support code, diagnostics support, fixtures, shaders, presets, and related assets are
      owned by the extracted project.
- [ ] Contract and consumer verification run against the installed public surface and installed asset
      package.
- [ ] Goggles consumes the extracted package through `find_package(...)` as the normal dependency
      path.
- [ ] The installed C and C++ public surfaces preserve the current post-retarget contract.

## Impact

- Affected modules: extracted library code under `src/render/chain`, `src/render/shader`,
  `src/render/texture`, required support currently under `src/util`, Goggles dependency wiring under
  `cmake/` and `src/render/`, and reusable filter-chain tests and fixtures now under `tests/render/`.
- Likely affected files: extracted project `CMakeLists.txt`, package config/export files, project-root
  `include/`, `src/`, `tests/`, `assets/`, and `cmake/` content, plus Goggles render/test wiring
  that switches to external package consumption.
- Impacted OpenSpec specs: `openspec/specs/goggles-filter-chain/spec.md`,
  `openspec/specs/build-system/spec.md`, `openspec/specs/filter-chain-c-api/spec.md`, and
  `openspec/specs/filter-chain-cpp-wrapper/spec.md`.
- Policy-sensitive areas: package/export correctness, ownership of support code and diagnostics,
  installed-surface verification quality, asset provenance, and maintaining one-way dependency
  direction between Goggles host code and the extracted library.

## Risks

- Hidden support and asset dependencies can surface only once the library builds outside Goggles.
- Installed-surface verification can expose assumptions currently masked by source-tree builds.
- Static and shared packaging can diverge if compile definitions, transitive dependencies, or asset
  lookup are not kept aligned.
- Goggles can drift back toward source coupling if external package consumption is not kept primary.

## Validation Plan

Verification contract:
- Standalone project proof:
  - configure, build, test, install, and export the extracted project from a clean checkout using
    its own documented CMake workflow
  - verify installed-surface contract tests and downstream consumer checks against the installed
    package
  - verify both `STATIC` and `SHARED` package consumption paths and confirm no `MODULE` target is
    part of the supported contract
- Goggles consumer proof:
  - configure and build Goggles against the installed package through `find_package(...)`
  - run Goggles host-side verification needed to confirm the preserved post-retarget boundary
- Pass criteria:
  - every Exact Success Criteria item is demonstrated with repository-local evidence
  - the extracted project owns the support and asset surface it requires
  - Goggles consumes the package externally without redefining the public contract
