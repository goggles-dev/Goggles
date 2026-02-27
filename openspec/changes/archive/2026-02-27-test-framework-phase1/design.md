## Context

Goggles has a fully-implemented headless mode (PR #96): `goggles --headless --frames N --output <path> -- <app>` runs the compositor and filter chain without a display and writes the final rendered frame as PNG via `VulkanBackend::readback_to_png()`. This is the execution primitive for all visual tests.

Phase 1 builds the three layers that visual tests require:
1. **Test clients** — deterministic Wayland apps that produce known pixel patterns as compositor input
2. **Image comparison** — a C++ library that can assert rendered PNG output against golden references or mathematical expectations
3. **CMake/CTest wiring** — unconditional build of test clients and image comparison library alongside the main project, with CTest label taxonomy (`unit`/`integration`/`visual`) for filtering

No new runtime code touches the production pipeline. All new code lives under `tests/`.

## Goals / Non-Goals

**Goals:**
- Provide `solid_color_client`, `gradient_client`, `quadrant_client`, `multi_surface_client` as deterministic source apps for headless tests
- Provide `CompareResult compare_images(actual, reference, tolerance)` usable in Catch2 tests and as a standalone CLI
- Wire up CTest label taxonomy (`unit`/`integration`/`visual`) so all test infrastructure builds unconditionally — no separate option or preset needed since wayland-client, stb_image, and Catch2 are already project dependencies
- Add a CTest smoke test that proves `Application::create_headless()` round-trips through the full pipeline

**Non-Goals:**
- Golden image files (created in Phase 2 once the framework exists)
- Phase 2–6 test content (aspect ratio tests, shader tests, compositor tests, CI integration)
- RenderDoc / GPU capture integration (Phase 3)
- SwiftShader integration (Phase 6)

## Decisions

### Decision 1: wl_shm for test clients (not Vulkan DMA-BUF)

**Chosen**: `wl_shm` shared memory buffers, CPU-rendered.

**Rationale**: Test clients are sources, not renderers. Their role is to deliver known pixel patterns into the compositor. `wl_shm` is synchronous, requires no GPU, and produces bit-exact pixel values regardless of hardware. Vulkan DMA-BUF from a test client would introduce GPU driver variance in the source content, defeating the purpose of deterministic golden-image testing. The goggles pipeline (DMA-BUF import → filter chain → offscreen image) is what's under test, not the client's rendering path.

**Alternative considered**: Vulkan + DMA-BUF clients. Rejected because: more code, GPU-dependent, not available on CI without a real device.

### Decision 2: stb_image for PNG I/O in the comparison library (no new dependency)

**Chosen**: Use `stb_image` for reading PNGs in `image_compare.cpp`.

**Rationale**: `stb_image` is already vendored in `packages/stb` and used via `stb_image_write_impl.cpp`. Adding a `stb_image_impl.cpp` TU in the test library avoids any new dependency. The comparison library only reads PNGs produced by `readback_to_png()` (always RGBA8 or RGB8 via stb_image_write), so a full-featured PNG library is not needed.

**Alternative considered**: libpng. Rejected: new dependency, no benefit for the narrow use case.

### Decision 3: CTest `add_test()` for headless smoke (not Catch2)

**Chosen**: Plain CTest `add_test()` wrapping the goggles binary directly.

**Rationale**: The smoke test is an end-to-end binary invocation (`goggles --headless --frames 5 --output <tmp> -- solid_color_client`). Wrapping this in a Catch2 fixture would require a test binary that calls `exec()` or `system()`, which adds complexity and a second process boundary. CTest `add_test()` is the standard CMake mechanism for exactly this pattern and gives clean pass/fail semantics with stdout/stderr capture.

**Alternative considered**: Catch2 with `GENERATE`/`SECTION` fixture. Rejected: unnecessary layer, harder to diagnose failures.

### Decision 4: Separate `tests/clients/` and `tests/visual/` subdirectories

**Chosen**: Two distinct directories under `tests/`.

**Rationale**: Clients and the comparison library have different consumers. Clients are binaries (used by CTest add_test and eventually pytest fixtures). The comparison library is a static library linked into Catch2 test TUs. Keeping them separate makes the CMake target graph clear and avoids conflating test infrastructure with test content.

### Decision 5: `CompareResult` as a plain struct (not `tl::expected`)

**Chosen**: `compare_images()` returns `CompareResult` directly; only PNG I/O operations return `Result<T>`.

**Rationale**: A comparison itself is not fallible — given two loaded images, it always produces a result. The `tl::expected` contract applies to operations that can fail at runtime (I/O, GPU calls). The fallible boundary is `load_png() -> Result<Image>`, which precedes comparison. This keeps call sites in test code clean (`auto result = compare_images(a, b, tol);`) without awkward error-handling boilerplate in assertions.

## Risks / Trade-offs

- **wl_shm client frame count timing**: The headless loop polls for frames at 1 ms intervals. If `solid_color_client` commits its first buffer before the compositor's Wayland socket is ready, the frame will be missed. **Mitigation**: clients retry `wl_display_dispatch()` until the first successful surface commit is acknowledged; the headless loop's 1 ms backoff handles transient latency.
- **stb_image alpha premultiplication**: stb_image can optionally premultiply alpha on load. **Mitigation**: always load with `stbi_set_unpremultiply_on_load(0)` and compare raw RGBA channels.
- **CTest working directory for smoke test**: The smoke test needs the goggles binary and `solid_color_client` binary both on `PATH` or via absolute paths. **Mitigation**: use CMake generator expressions (`$<TARGET_FILE:goggles>`) in `add_test()` to inject absolute paths.
- **CMake option footprint**: All visual test targets now build unconditionally. This is correct because all dependencies (wayland-client, wayland-protocols, stb_image, Catch2) are already hard requirements of the main goggles build. CTest labels (`unit`/`integration`/`visual`) provide the filtering mechanism for selective test execution.

## Open Questions

None blocking Phase 1. Phase 2 open question: should golden images be stored via Git LFS or as untracked binary blobs committed directly? (LFS preferred but requires repo-level setup.)
