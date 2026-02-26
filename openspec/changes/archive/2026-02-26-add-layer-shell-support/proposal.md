## Why

The compositor does not implement `wlr-layer-shell-unstable-v1`, so game launcher overlays (Steam
Big Picture, Epic Games overlay) and desktop panels that depend on this protocol cannot connect or
render. This blocks Goggles from being usable as an overlay tool for overlay-heavy workflows.

## What Changes

- Implement `wlr_layer_shell_v1` protocol support in `src/compositor/compositor_server.cpp`
- Register and create the `wlr_layer_shell_v1` global on the compositor Wayland display
- Track layer surfaces with per-surface lifecycle hooks (commit, map, unmap, destroy)
- Configure layer surfaces on first commit with geometry derived from anchor/margin/size state
- Render layer surfaces in protocol-defined layer order around the primary capture surface
- Route keyboard focus to layer surfaces that request `exclusive` interactivity, without changing the capture target
- Handle `xdg_popup` children spawned from layer surfaces via the existing popup infrastructure

## Capabilities

### New Capabilities

- `layer-shell-overlay`: `wlr-layer-shell-unstable-v1` protocol support — creates the global,
  tracks `LayerSurfaceHooks`, configures geometry, and exposes layer surfaces as render-only
  overlays distinct from the primary capture target.

### Modified Capabilities

- `compositor-capture`: Render ordering now includes layer surface passes (background, bottom
  before the primary surface; top, overlay after it) in the compositor-presented DMA-BUF frame.
- `input-forwarding`: Keyboard focus may transfer to a mapped layer surface that requests
  `exclusive` interactivity without changing the surface used for frame capture or surface
  enumeration.

## Impact

- **`src/compositor/compositor_server.cpp`** — all implementation; adds ~200–300 lines
- **`src/compositor/compositor_server.hpp`** — no public API changes; layer surfaces are render-only overlays, not enumerated in `get_surfaces()`
- **No new CMake targets** — `wlr_layer_shell_v1.h` is part of the already-linked `PkgConfig::wlroots`
- **Policy-sensitive impacts:**
  - Ownership: `LayerSurfaceHooks` are heap-allocated and destroyed in their `layer_destroy` signal handler (RAII-equivalent; no raw `new`/`delete` invariant preserved via explicit delete in handler)
  - Threading: all layer shell event handlers run on the compositor thread; `hooks_mutex` guards `layer_hooks` as with existing hook vectors
  - Error handling: `setup_layer_shell()` returns `Result<void>` via `tl::expected`; setup failure propagates to `start()` via `GOGGLES_TRY`
  - Logging: layer shell lifecycle events use `GOGGLES_LOG_DEBUG`; no `error`/`critical` unless setup fails
