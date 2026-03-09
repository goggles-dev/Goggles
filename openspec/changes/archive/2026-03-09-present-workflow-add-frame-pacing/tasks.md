## 1. Shared pacing target and runtime plumbing

- [x] 1.1 Extend the runtime pacing state so one effective `render.target_fps` value can be read and
  updated through the application boundary without splitting compositor and viewer policy.
- [x] 1.2 Thread the effective pacing target from config/CLI startup state into both
  `input::CompositorServer` and `render::VulkanBackend`, preserving `target_fps = 0` as uncapped.
- [x] 1.3 Add one runtime update path in `src/app/application.cpp` that applies pacing changes to the
  compositor and viewer together.

## 2. Compositor capture pacing

- [x] 2.1 Add compositor-owned pacing state for the active capture target in `src/compositor/` so
  callback/publication flow no longer depends only on immediate commit-triggered `frame_done`.
- [x] 2.2 Update the XDG, XWayland, and layer-shell capture paths to use the compositor pacing policy
  while keeping uncapped mode explicit.
- [x] 2.3 Verify the compositor pacing path preserves active-target behavior, present reset behavior,
  and compatibility across Wayland-hosted and X11-hosted sessions.

## 3. Viewer reuse and Application window controls

- [x] 3.1 Reuse the existing `src/render/backend/render_output.cpp` present-wait / throttle logic as
  the viewer half of the shared pacing contract.
- [x] 3.2 Add Application-window runtime pacing controls in `src/ui/imgui_layer.*` that reflect the
  current effective target and support uncapped mode.
- [x] 3.3 Wire Application-window pacing changes through `src/app/application.cpp` so compositor and
  viewer updates are applied together without restart.

## 4. Verification

- [x] 4.1 Verify the implementation matches the `render-pipeline`, `compositor-capture`, and
  `app-window` delta specs and preserves the one-global-target contract.
- [x] 4.2 Add or update targeted automated coverage for pacing configuration and runtime update
  plumbing where repository test surfaces allow bounded coverage.
- [x] 4.3 Run `pixi run build -p debug`, `pixi run build -p asan`, `pixi run test -p asan`, and
  `pixi run build -p quality`.
- [x] 4.4 Run host-sensitive pacing validation for Wayland and X11 when the local runtime supports
  both; otherwise record a named manual fallback with target, host type, observed FPS window, and
  proof location.
