# Implementation Handoff

Change: `refactor-compositor-server-modules`

## Touched compositor files

- `src/compositor/CMakeLists.txt`
- `src/compositor/compositor_server.cpp`
- `src/compositor/compositor_server.hpp`
- `src/compositor/compositor_state.hpp`
- `src/compositor/compositor_targets.hpp`
- `src/compositor/compositor_protocol_hooks.hpp`
- `src/compositor/compositor_core.cpp`
- `src/compositor/compositor_cursor.cpp`
- `src/compositor/compositor_focus.cpp`
- `src/compositor/compositor_input.cpp`
- `src/compositor/compositor_layer_shell.cpp`
- `src/compositor/compositor_present.cpp`
- `src/compositor/compositor_xdg.cpp`
- `src/compositor/compositor_xwayland.cpp`

## Approved boundaries and close variants

- `compositor_server.*` is reduced to the public facade and startup delegation.
- `compositor_core.*` contains startup, shutdown, backend/output/event-loop orchestration, compositor thread lifecycle, and teardown.
- `compositor_xdg.*`, `compositor_xwayland.*`, `compositor_input.*`, `compositor_focus.*`, `compositor_cursor.*`, `compositor_present.*`, and `compositor_layer_shell.*` hold the extracted subsystem logic.
- Shared ownership remains centralized on `CompositorState` in `src/compositor/compositor_state.hpp`.
- No materially different module boundary was required; no close-variant divergence needs spec/design reconciliation.

## Preserved quirks and workarounds

- XWayland input re-activation before each key/pointer event remains in `src/compositor/compositor_xwayland.cpp:94`.
- The no-destroy-listener rule on `xsurface->surface` remains documented and enforced in `src/compositor/compositor_xwayland.cpp:301`.
- Stable hook allocation for XDG, XWayland, and layer-shell listeners remains adjacent to each subsystem in:
  - `src/compositor/compositor_xdg.cpp:53`
  - `src/compositor/compositor_xdg.cpp:116`
  - `src/compositor/compositor_xwayland.cpp:206`
  - `src/compositor/compositor_layer_shell.cpp:101`
- Layer-shell `new_popup` forwarding into XDG-owned popup hooks remains in `src/compositor/compositor_layer_shell.cpp:155`.
- Layer-shell forwarded popup owner-root unconstraining remains in `src/compositor/compositor_xdg.cpp:179`.

## Behavior-preservation checklist

- startup/shutdown order: `src/compositor/compositor_server.cpp:28`, `src/compositor/compositor_core.cpp:286`
- Wayland client handling: `src/compositor/compositor_xdg.cpp:17`, `src/compositor/compositor_xdg.cpp:43`
- XWayland client handling: `src/compositor/compositor_xwayland.cpp:119`, `src/compositor/compositor_xwayland.cpp:202`
- input forwarding: `src/compositor/compositor_input.cpp:364`, `src/compositor/compositor_input.cpp:378`, `src/compositor/compositor_input.cpp:411`, `src/compositor/compositor_input.cpp:433`
- focus targeting: `src/compositor/compositor_focus.cpp:215`, `src/compositor/compositor_focus.cpp:268`, `src/compositor/compositor_focus.cpp:673`
- pointer constraints (activation, confine, cursor hints): `src/compositor/compositor_focus.cpp:188`, `src/compositor/compositor_focus.cpp:204`
- layer-shell render ordering, popup routing, and exclusive keyboard focus: `src/compositor/compositor_layer_shell.cpp:155`, `src/compositor/compositor_focus.cpp:427`, `src/compositor/compositor_focus.cpp:529`
- cursor visibility/behavior: `src/compositor/compositor_cursor.cpp:92`, `src/compositor/compositor_cursor.cpp:202`
- presented-frame acquisition/export: `src/compositor/compositor_present.cpp:93`, `src/compositor/compositor_present.cpp:190`

## Verification results

- `pixi run build -p debug`: pass
- `pixi run build -p asan`: pass
- `pixi run test -p asan`: pass
- `pixi run build -p quality`: pass
- `ctest --preset asan -R "goggles_tests|goggles_test_child_death_signal|goggles_test_headless_child_exit" --output-on-failure`: pass
- `ctest --preset asan -R "goggles_auto_input_forwarding_(x11|wayland)" --output-on-failure`: pass
- `ctest --preset asan -R "goggles_headless_integration(_png_exists)?" --output-on-failure`: pass
- `grep -R --line-number '\bthrow\b' src/compositor --include='*.cpp' --include='*.hpp'`: no matches
- `grep -R --line-number 'std::thread\|std::jthread' src/compositor --include='*.cpp' --include='*.hpp'`: only `std::jthread` in `src/compositor/compositor_core.cpp:280` and `src/compositor/compositor_state.hpp:153`
- `grep -R --line-number 'Vk[A-Za-z0-9_]*' src/compositor --include='*.cpp' --include='*.hpp'`: no matches
- `lsp_diagnostics` on touched compositor sources: no diagnostics

## Skipped checks and fallback evidence

- No checks were skipped.
- Manual fallback evidence was not needed because both automated input-forwarding checks ran successfully.

## Quirk-to-subsystem proof map

- XWayland re-activation before input: `src/compositor/compositor_xwayland.cpp:94`
- No destroy listener on `xsurface->surface`: `src/compositor/compositor_xwayland.cpp:301`
- Stable XDG hook allocation: `src/compositor/compositor_xdg.cpp:53`, `src/compositor/compositor_xdg.cpp:116`
- Stable XWayland hook allocation: `src/compositor/compositor_xwayland.cpp:206`
- Stable layer-shell hook allocation: `src/compositor/compositor_layer_shell.cpp:101`
- Layer-shell popup forwarding into XDG-owned popup hooks: `src/compositor/compositor_layer_shell.cpp:155`
