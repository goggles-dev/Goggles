## 1. Scaffold — Include and Struct Definitions

- [x] 1.1 Add `#include <wlr/types/wlr_layer_shell_v1.h>` inside the `extern "C"` block in `compositor_server.cpp`
- [x] 1.2 Add `LayerSurfaceHooks` struct to `CompositorServer::Impl` with all fields: `impl`, `layer_surface`, `surface`, `id`, `layer`, `configured`, `mapped`, and six `wl_listener` members (`surface_commit`, `surface_map`, `surface_unmap`, `surface_destroy`, `layer_destroy`, `new_popup`)
- [x] 1.3 Add `wlr_layer_shell_v1* layer_shell = nullptr` and `std::vector<LayerSurfaceHooks*> layer_hooks` to `CompositorServer::Impl`
- [x] 1.4 Add `wl_listener new_layer_surface{}` to `Impl::Listeners`

## 2. Setup and Teardown

- [x] 2.1 Implement `CompositorServer::Impl::setup_layer_shell() -> Result<void>`: call `wlr_layer_shell_v1_create(display, 4)`, error on null, init and register `listeners.new_layer_surface` via `wl_list_init` + lambda + `wl_signal_add`
- [x] 2.2 Call `GOGGLES_TRY(impl.setup_layer_shell())` in `CompositorServer::start()` after `setup_xdg_shell()`
- [x] 2.3 In `CompositorServer::stop()`: call `detach_listener(impl.listeners.new_layer_surface)` and set `impl.layer_shell = nullptr`

## 3. New Layer Surface Handler

- [x] 3.1 Implement `Impl::handle_new_layer_surface(wlr_layer_surface_v1*)`: assign `layer_surface->output = output` if null, allocate `LayerSurfaceHooks`, assign `id` and `layer` from `pending.layer`, set `surface = layer_surface->surface`
- [x] 3.2 In `handle_new_layer_surface`: init and register all six listeners using the `offsetof(Impl::Listeners, ...)` lambda pattern; push hooks to `layer_hooks` under `hooks_mutex`

## 4. Commit Handler — Initial Configure

- [x] 4.1 Implement `Impl::handle_layer_surface_commit(LayerSurfaceHooks*)`: if `!hooks->configured && layer_surface->initial_commit`, compute `width`/`height` per design D2 geometry rules (fully-anchored → output size; partial → mix of output dimension and `desired_width`/`desired_height`), call `wlr_layer_surface_v1_configure()`, set `hooks->configured = true`
- [x] 4.2 On subsequent commits (already configured): call `wlr_surface_send_frame_done()` on `hooks->surface` to unblock the client

## 5. Map / Unmap Handlers

- [x] 5.1 Implement `Impl::handle_layer_surface_map(LayerSurfaceHooks*)`: set `mapped = true`; if `layer_surface->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE`, call `wlr_seat_keyboard_enter()` for `hooks->surface`; call `request_present_reset()`
- [x] 5.2 Implement `Impl::handle_layer_surface_unmap(LayerSurfaceHooks*)`: set `mapped = false`; if keyboard was given to this surface (`seat->keyboard_state.focused_surface == hooks->surface`), restore keyboard focus to `focused_surface` via `wlr_seat_keyboard_enter()`; call `request_present_reset()`

## 6. Destroy Handler

- [x] 6.1 Implement `Impl::handle_layer_surface_destroy(LayerSurfaceHooks*)`: detach all six listeners via `detach_listener()`; if `seat->keyboard_state.focused_surface == hooks->surface` and `focused_surface` is non-null, restore keyboard focus; remove hooks from `layer_hooks` under `hooks_mutex`; `delete hooks`

## 7. Layer Surface Popup Forwarding

- [x] 7.1 In the `new_popup` listener registered on `layer_surface->events.new_popup`, forward the `wlr_xdg_popup*` to `handle_new_xdg_popup()`

## 8. Render Integration

- [x] 8.1 Implement `Impl::render_layer_surfaces(wlr_render_pass*, zwlr_layer_shell_v1_layer)`: iterate `layer_hooks` under `hooks_mutex`, skip unmapped or null entries, compute position from `current.anchor` and `current.margin` relative to the output, call `wlr_layer_surface_v1_for_each_surface()` with `render_surface_iterator`
- [x] 8.2 In `render_surface_to_frame()`, insert layer render calls in the correct order: `render_layer_surfaces(pass, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND)` and `BOTTOM` before `render_root_surface_tree()`; `TOP` and `OVERLAY` after XWayland popup rendering and before `render_cursor_overlay()`

## 9. Verification

- [x] 9.1 Build with quality preset and confirm zero clang-tidy warnings: `pixi run build -p quality`
- [x] 9.2 Run full test suite: `pixi run test -p test` (pre-existing X11 test failure confirmed on unmodified `main` — not caused by this change)
- [x] 9.3 Manual smoke test: launch `pixi run dev` and connect a client that uses `zwlr_layer_shell_v1` (e.g., `wlr-layer-shell-example` or `waybar` against the goggles socket); confirm layer surface appears in the compositor-presented frame
