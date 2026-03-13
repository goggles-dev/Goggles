# Design: Normalize Filter Chain Consumer Boundary

## Technical Approach

This change captures the implemented PR 1 state as a Goggles consumer-boundary cleanup. Goggles keeps
`goggles_render`, `VulkanBackend`, and `FilterChainController` on the stable public
`goggles-filter-chain` boundary while the build resolves the implementation from a normalized provider
seam. The runtime contract stays post-retarget: the host owns swapchain, presentation, import, and
submission; the filter-chain boundary owns preset-derived runtime state and output-state rebuild on
retarget.

The implementation already reflects this boundary in three places. First, boundary-owned public
headers live under `src/render/chain/include/goggles/filter_chain/` and compatibility forwarders keep
older monorepo include paths working. Second, `cmake/GogglesFilterChainProvider.cmake` normalizes
provider resolution so Goggles always links `goggles-filter-chain` in `in-tree`, `subdir`, or
`package` mode. Third, tests distinguish reusable contract coverage from Goggles-owned host/backend
coverage, and the package rehearsal exercises the installed boundary inside the monorepo.

## Architecture Decisions

### Decision: Keep Goggles host code on the stable wrapper and boundary headers

**Choice**: Backend-facing code uses `goggles_filter_chain.hpp`,
`goggles/filter_chain/filter_controls.hpp`, and `goggles/filter_chain/vulkan_context.hpp` instead of
concrete chain internals.

**Rationale**: The implemented controller and backend code already preserve one-way dependency flow
from Goggles host code into `goggles-filter-chain`. Keeping that boundary explicit is the core PR 1
outcome.

### Decision: Preserve the post-retarget ownership contract as the consumer baseline

**Choice**: `VulkanBackend` continues to own swapchain recreation, presentation, external image
import, synchronization, submit, and present. `FilterChainController` continues to orchestrate active
and pending runtimes through `FilterChainRuntime`. The boundary runtime preserves preset identity,
controls, and source-independent state across output retargets.

**Rationale**: This is the verified behavior encoded in `FilterChainController`,
`VulkanBackend::recreate_swapchain(...)`, and the contract tests. PR 1 names and preserves that
behavior instead of changing it.

### Decision: Own public support contracts at the filter-chain boundary

**Choice**: Public support types such as `Error`, `Result`, `ScaleMode`, filter controls, Vulkan
context, and diagnostics entry headers live under `src/render/chain/include/goggles/filter_chain/`.
Older source-tree includes remain available through compatibility forwarders such as
`src/render/chain/filter_controls.hpp`, `src/render/chain/vulkan_context.hpp`, and
`src/util/error.hpp`.

**Rationale**: The current code already removed Goggles-only `src/util/*` dependencies from the
public consumer surface while keeping in-tree callers buildable.

### Decision: Normalize provider resolution to a single target identity inside Goggles

**Choice**: `cmake/GogglesFilterChainProvider.cmake` resolves `goggles-filter-chain` from:
- `in-tree`
- `subdir`
- `package`

If an external source exposes a different target name, the provider module creates a local normalized
target named `goggles-filter-chain`.

**Rationale**: Goggles consumer code and tests should not branch on target naming. The implemented
provider module already centralizes that decision.

### Decision: Split reusable contract coverage from Goggles host coverage

**Choice**: `tests/CMakeLists.txt` keeps reusable contract coverage in
`goggles_filter_chain_contract_tests` and Goggles-owned host/backend coverage in `goggles_tests`.
Package consumer rehearsal keeps Goggles host coverage active while contract tests remain attached to
the in-tree authored boundary target.

**Rationale**: This matches the current code reality and prevents Goggles backend/presentation
expectations from being treated as reusable library behavior.

### Decision: Treat package/install flow as a monorepo rehearsal only

**Choice**: `src/render/CMakeLists.txt`, `cmake/GogglesFilterChainConfig.cmake.in`,
`CMakePresets.json`, `pixi.toml`, and `scripts/task/rehearse-filter-chain-provider.sh` support an
installed package rehearsal for Goggles consumption inside this repository.

**Rationale**: The current implementation proves Goggles can consume the normalized boundary through
its own install/export path, but PR 1 does not claim broader standalone package readiness.

## Data Flow

### Consumer boundary flow

```text
VulkanBackend
  -> FilterChainController
      -> FilterChainRuntime (C++ wrapper)
          -> C ABI + boundary-owned public headers
              -> chain/shader/texture implementation
```

### Output-format retarget flow

```text
source format change
  -> VulkanBackend recreates host-owned swapchain resources
  -> FilterChainController updates authoritative output target
  -> FilterChainRuntime::retarget_output(format)
  -> FilterChainRuntime::handle_resize(extent)
  -> boundary preserves preset identity, controls, and source-independent runtime state
```

### Provider resolution flow

```text
Goggles configure
  -> GogglesFilterChainProvider.cmake
  -> in-tree | subdir | package
  -> normalize target identity to `goggles-filter-chain`
  -> link goggles_render and contract tests through that boundary
```

## File Changes

| File | Role in PR 1 state |
|------|--------------------|
| `cmake/GogglesFilterChainProvider.cmake` | Normalizes provider selection and target identity. |
| `src/render/CMakeLists.txt` | Consumes provider output, installs boundary headers, and stages package config when Goggles owns the target. |
| `src/render/chain/include/goggles/filter_chain/*.hpp` | Boundary-owned public support headers. |
| `src/render/chain/filter_controls.hpp` | Compatibility forwarder to boundary-owned filter controls header. |
| `src/render/chain/vulkan_context.hpp` | Compatibility forwarder to boundary-owned Vulkan context header. |
| `src/util/error.hpp` | Compatibility forwarder to boundary-owned error/result contract. |
| `src/render/backend/filter_chain_controller.hpp` | Host controller stays on wrapper and boundary-owned headers. |
| `src/render/backend/filter_chain_controller.cpp` | Preserves post-retarget active/pending runtime orchestration on the wrapper boundary. |
| `tests/CMakeLists.txt` | Splits reusable contract coverage from Goggles host/backend coverage and wires package-mode behavior. |
| `scripts/task/rehearse-filter-chain-provider.sh` | Rehearses in-tree and package consumer modes inside the monorepo. |

## Interfaces / Contracts

### Stable consumer target contract

```cmake
goggles_resolve_filter_chain_provider()

target_link_libraries(goggles_render PUBLIC goggles-filter-chain)
```

Goggles consumer code never changes target names across supported provider modes.

### Stable public include contract inside Goggles

Canonical public surface:

```text
include/
├── goggles_filter_chain.h
├── goggles_filter_chain.hpp
└── goggles/
    └── filter_chain/
        ├── error.hpp
        ├── filter_controls.hpp
        ├── result.hpp
        ├── scale_mode.hpp
        ├── vulkan_context.hpp
        └── diagnostics/*.hpp
```

Compatibility forwarders remain available in the monorepo, but Goggles host code is expected to stay
on the canonical boundary-owned headers.

### Test ownership contract

- `goggles_filter_chain_contract_tests` validates boundary behavior and public-surface hygiene.
- `goggles_tests` validates Goggles-owned backend, controller, and presentation behavior.
- package consumer rehearsal keeps host coverage active and does not claim standalone contract-test
  portability outside Goggles.

## Validation Strategy

- Build the normal developer paths with `pixi run build -p debug` and `pixi run build -p quality`.
- Run reusable and host coverage with `ctest --preset test -R "^(goggles_unit_tests|goggles_filter_chain_contract_tests)$" --output-on-failure`.
- Rehearse normalized provider consumption with `pixi run rehearse-filter-chain-provider`.
- Use source-audit tests to confirm backend code stays on wrapper and boundary-owned headers.

## Open Questions

- None blocking for PR 1. Follow-on work can revisit what additional proof is needed for standalone
  repository or external downstream readiness.
