## Context

`CompositorServer` is a single-file headless wlroots compositor (~3 000 lines in
`compositor_server.cpp`) that implements Wayland protocol support for game input forwarding and
surface capture. Existing protocol support follows a uniform pattern: a `setup_*()` function
creates the wlroots global, registers `wl_listener` callbacks on an `Impl::Listeners` struct, and
per-surface state is heap-allocated as a `*Hooks` struct that carries its own `wl_listener`
members for lifecycle signals.

The compositor uses a single headless output with no panel/desktop shell, so there is no existing
exclusive-zone arrangement system or output-layout-aware geometry resolver.

Frame export is driven by `render_surface_to_frame()`, which opens a wlr render pass, composites
the primary capture surface tree via `render_root_surface_tree()`, optionally adds XWayland
override-redirect popups, then overlays the software cursor. The resulting buffer is exported as a
DMA-BUF for the viewer.

## Goals / Non-Goals

**Goals:**
- Expose `wlr-layer-shell-unstable-v1` v4 on the compositor Wayland display
- Track mapped layer surfaces per-surface lifecycle (configure, map, unmap, destroy)
- Render layer surfaces in protocol layer order around the primary capture surface in
  `render_surface_to_frame()`
- Forward keyboard focus to layer surfaces that request `exclusive` interactivity without
  changing the capture target
- Route `xdg_popup` children of layer surfaces through the existing popup infrastructure

**Non-Goals:**
- Full exclusive-zone arrangement (panel reservations, usable-area shrinking) — not needed with a
  single headless output and no desktop shell
- Exposing layer surfaces in `get_surfaces()` — they are render-only overlays, not selectable
  capture targets
- Supporting layer surface resize requests (`set_size`, `set_anchor` after map) — game launcher
  overlays are configured once at map time
- Input hit-testing for pointer delivery to layer surfaces — pointer events always go to the
  primary capture surface

## Decisions

### D1: Layer surfaces are render-only overlays, excluded from `get_surfaces()`

**Decision**: `layer_hooks` is never iterated in `get_surfaces()` or `focus_surface_by_id()`.

**Rationale**: Layer surfaces are transient UI decorations (overlays, panels). Including them in
the surface selector would confuse users who would then try to "capture" an overlay. The capture
path remains anchored to XDG toplevel / XWayland surfaces.

**Alternative considered**: Add a `is_layer_surface` flag to `SurfaceInfo` and let the UI filter.
Rejected — adds UI complexity for no user benefit; omission is simpler and correct.

---

### D2: Simplified geometry — full output for fully-anchored, edge size for partially-anchored

**Decision**: On first commit, compute configure dimensions as follows:
- If all four anchor edges are set (`ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | BOTTOM | LEFT | RIGHT`):
  send output width × height.
- If only horizontal edges (top or bottom) with left+right: send output width × `desired_height`
  (or a sensible default).
- If only vertical edges (left or right) with top+bottom: send `desired_width` × output height.
- Otherwise: send `desired_width` × `desired_height` (clamped to non-zero output dimensions).

**Rationale**: Game launcher overlays use full-screen or full-width configurations. A full
exclusive-zone layout engine is ~500 lines and not needed for a single headless output.

**Alternative considered**: Implement the full layer arrangement algorithm from sway. Rejected as
out of scope; can be added later if needed.

---

### D3: Keyboard focus is separate from the capture target

**Decision**: When a layer surface with `exclusive` keyboard interactivity maps, call
`wlr_seat_keyboard_enter()` on its `wlr_surface`. When it unmaps or is destroyed, restore
keyboard focus to `focused_surface` (the current capture target). The `focused_surface` /
`focused_xsurface` pointers that drive frame capture are NOT changed.

**Rationale**: The capture pipeline is driven by `focused_surface` / `focused_xsurface`. If
an overlay steals keyboard focus for text input (e.g., a search field in Steam Big Picture), the
game's frame should still be what is captured and displayed. Conflating keyboard focus with the
capture target would break the capture path.

**Alternative considered**: Change `focused_surface` to the layer surface for the duration of
keyboard focus. Rejected — `render_surface_to_frame()` and `get_input_target()` both read
`focused_surface` to determine what to render as the primary frame.

---

### D4: `LayerSurfaceHooks` follows the existing `*Hooks` pattern exactly

**Decision**: `LayerSurfaceHooks` is heap-allocated in `handle_new_layer_surface()` and
self-destructs in its own `layer_destroy` handler (same pattern as `XdgToplevelHooks`).

**Rationale**: Consistency with the existing codebase. The hooks are added to
`layer_hooks : std::vector<LayerSurfaceHooks*>` guarded by `hooks_mutex`, identical to
`xdg_hooks` and `xwayland_hooks`.

---

### D5: Layer surface popups delegate to existing `handle_new_xdg_popup()`

**Decision**: Connect `layer_surface->events.new_popup` and forward the `wlr_xdg_popup*` to the
existing `handle_new_xdg_popup()`. No separate popup tracking for layer surfaces.

**Rationale**: `XdgPopupHooks` already handles popup lifecycle, configure, render, and destroy.
The parent surface context for positioning differs, but because goggles composites everything into
a flat render pass, offset computation is the key concern — and the existing popup offset logic
is based on the popup's own `positioner`, which is self-contained.

## Risks / Trade-offs

**[Risk] Keyboard focus restoration on crash/unmap may miss edge cases** →
Mitigation: restore `focused_surface` keyboard focus in both `surface_unmap` and `layer_destroy`
handlers. Add a null check before `wlr_seat_keyboard_enter()` in all restoration paths.

**[Risk] Popup geometry offset wrong for layer surface parents** →
Mitigation: layer surface popups use `wlr_xdg_popup_get_position()` from the positioner, which
is compositor-position-agnostic. Since the headless output has no offset, popup positions are
in output coordinates, which match the render pass coordinates. Verify with Steam overlay in
manual testing.

**[Risk] wlroots 0.19 `initial_commit` field may not be set on first commit** →
Mitigation: read the `initial_commit` field from `wlr_layer_surface_v1` at commit time (same
field used by sway and river for wlroots 0.19). If already configured (`hooks->configured`),
skip the configure call to avoid protocol errors.

## Open Questions

None — all decisions are resolved above.
