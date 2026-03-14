# Exploration: Phase 3-6 Standalone Filter-Chain Extraction

## Current State

Phase 1-2 monorepo groundwork is complete and archived. The `goggles-filter-chain` target is
decoupled at the public header level — 5 public headers under `include/goggles/filter_chain/` are
self-contained, internal headers use canonical paths, and no PUBLIC linkage to `goggles_util`
remains. The target composes 5 OBJECT libraries (chain, shader, texture, diagnostics, logging) into
a single STATIC or SHARED artifact with a `GogglesFilterChain::goggles-filter-chain` ALIAS ready
for future `find_package()` consumption.

## Remaining Coupling (must sever for standalone build)

### 1. util/logging.hpp and util/profiling.hpp (13 .cpp files each)
Every implementation file in chain/, shader/, texture/ includes both. These are lightweight facades
over spdlog and Tracy. The standalone project needs its own logging/profiling shims.

### 2. util/serializer.hpp (1 .cpp file)
Only `shader/shader_runtime.cpp`. Small serialization utility for shader cache.

### 3. Diagnostics implementation (util/diagnostics/)
The `goggles_diagnostics` OBJECT library (4 .cpp files) is already bundled into the filter-chain
artifact. However, it currently links `goggles_util` which transitively pulls in `toml11`,
`BS_thread_pool`, and `Threads` — none needed by the filter chain. This transitive coupling must be
severed: the diagnostics code needs to be self-contained under standalone ownership.

Chain/shader headers reference 10 distinct diagnostics headers (`diagnostic_policy.hpp`,
`diagnostic_session.hpp`, `gpu_timestamp_pool.hpp`, `compile_report.hpp`,
`source_provenance.hpp`, `diagnostic_event.hpp`, `log_sink.hpp`, `diagnostic_sink.hpp`,
`test_harness_sink.hpp`, `binding_ledger.hpp`).

### 4. Include path coupling
PRIVATE include dirs `${CMAKE_SOURCE_DIR}/src` and `${CMAKE_SOURCE_DIR}/src/render` are used by
the filter-chain target. All internal `#include <render/chain/...>` and `<util/...>` paths assume
this layout.

## Test Split

**Contract tests (move to standalone — ~22 files):**
- C API contracts, retarget contract, filter controls, chain resources, preset parser,
  retroarch preprocessor, slang reflect, shader runtime, semantic binder, zfast integration,
  runtime diagnostics, GPU timestamp pool, 10+ diagnostics unit tests

**Host integration tests (stay in Goggles — 3 files):**
- `test_filter_boundary_contracts.cpp` (reads VulkanBackend/Controller source)
- `test_vulkan_backend_subsystem_contracts.cpp` (tests controller as backend subsystem)
- `test_filter_chain_retarget.cpp` (tests via FilterChainController)

## Asset Inventory

Three asset categories need standalone ownership:

1. **Test shader fixtures** (`shaders/retroarch/test/`): format.slangp, history.slangp,
   feedback.slangp, frame_count.slangp, pragma-name.slangp, decode-format.slang (~12 files)
2. **Diagnostics test corpus** (`tests/util/test_data/filter_chain_diagnostics/`): authoring
   corpus (valid/invalid/reflection), semantic probes (~14 files)
3. **Real upstream shaders** (`shaders/retroarch/crt/`, etc.): used by zfast integration and
   shader validation. The standalone project should own a curated test subset, not the full mirror.

## Third-Party Dependencies

| Library | Link | Source | Standalone needs |
|---------|------|--------|------------------|
| Vulkan::Vulkan | PUBLIC | System SDK | find_package(Vulkan) |
| nonstd::expected-lite | PUBLIC | Pixi/conda | find_package(expected-lite) |
| spdlog::spdlog | PRIVATE | Pixi/conda | find_package(spdlog) |
| slang::slang | PRIVATE | Pixi/conda | find_package(slang CONFIG) |
| stb_image | PRIVATE | Pixi/conda (header-only) | INTERFACE target or find_package |
| Catch2 | TEST | Pixi/conda | find_package(Catch2) |

All are resolved via standard `find_package()` — no Goggles-specific Find modules needed. The
standalone `cmake/FilterChainDependencies.cmake` can wrap these cleanly.

## Approaches

### Approach A: Sibling directory extraction (recommended)

Create the standalone project as a sibling directory (e.g., `../goggles-filter-chain/`) with its
own git repo. Copy sources (not move — keep Goggles building during transition), establish the
standalone build, prove it works, then switch Goggles to `find_package()` consumption and remove
the in-repo copies.

- Pros: Clean separation from day one, standalone CI possible immediately, Goggles never breaks
  during transition
- Cons: Temporary code duplication during transition, two repos to manage
- Effort: High (but matches the design goal of "independent project")

### Approach B: In-repo subdirectory then extract

Create `filter-chain/` inside the Goggles repo as a self-contained CMake subproject. Prove it
builds standalone via `cmake -S filter-chain/`. Then move to its own repo.

- Pros: Single repo during development, easier to iterate, atomic commits
- Cons: Muddies "standalone" claim until final extraction, risk of re-coupling
- Effort: Medium

### Approach C: Incremental in-place extraction

Keep sources where they are but incrementally make the existing OBJECT libraries buildable from a
standalone CMakeLists.txt that references `src/render/chain/` etc. relative to a different root.

- Pros: No file moves until the very end
- Cons: Fragile path assumptions, doesn't prove standalone ownership, hard to verify
- Effort: Low initially, high to finalize

## Recommendation

**Approach B** (in-repo subdirectory) provides the best balance for this workspace. It keeps
everything in one git history for review, allows atomic commits that move files + update consumers,
and still produces a provably standalone CMake project. The final repo extraction is a clean
`git subtree split` or directory copy once verification passes.

The key ordering constraint: sever the diagnostics → goggles_util coupling FIRST (Phase 4.3),
because that transitive dependency on toml11/BS_thread_pool is the deepest remaining coupling.
Everything else (logging shim, profiling shim, serializer) is straightforward.

## Risks

- Diagnostics decoupling may require interface changes if diagnostic types reference goggles_util
  types (config, job_system) — needs careful audit
- Shader validation tests scan the full `shaders/retroarch/` mirror — the standalone project
  should own only a curated subset to avoid mirroring upstream content
- The `test-shared` transitional presets must not be removed until the standalone project proves
  both STATIC and SHARED consumption — ordering matters

## Open Questions

- Where should the standalone project root live during development? (sibling dir vs in-repo subdir)
- Should the standalone project get its own Pixi environment or rely on system/vcpkg for deps?

## Ready for Proposal

Yes — with a decision on project location. Recommend asking the user.
