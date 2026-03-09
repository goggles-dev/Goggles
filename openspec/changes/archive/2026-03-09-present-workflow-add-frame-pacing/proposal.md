## Why

`pixi run start -- vkcube` can currently drive the nested compositor and viewer at extremely high
frame rates because the compositor-side present path does not pace client callbacks while the viewer
only paces its own final presentation. That wastes power, makes the end-to-end cadence depend on the
host compositor, and breaks the expectation that one target FPS governs the whole Goggles session.

## Problem

- `render.target_fps` currently paces the viewer backend, but it does not govern compositor callback
  cadence for captured clients.
- The nested compositor can still issue `frame_done` as fast as commits arrive, so the target app may
  free-run even when the viewer is paced.
- The Application window exposes performance metrics but no runtime control for the pacing target,
  leaving config and CLI as the only control surfaces.
- Wayland-hosted and X11-hosted sessions need one consistent pacing contract for this first version.

## Scope

- Extend pacing from the current viewer-only behavior into the full `app -> compositor -> viewer`
  present workflow.
- Keep one global pacing target for v1 instead of stage-specific targets.
- Reuse the existing viewer present-wait and CPU-throttle subsystem where possible rather than
  introducing a second independent pacing policy.
- Add Application-window runtime controls for pacing that stay aligned with the existing
  config/CLI-driven `render.target_fps` contract.
- Define acceptance around FPS tolerance only for v1: +/-3 FPS over 10 seconds on both Wayland and
  X11 hosts.

## What Changes

- Update render-pipeline requirements so `render.target_fps` becomes the authoritative pacing target
  for the full present workflow, not only the viewer's final swapchain present.
- Update compositor-capture requirements so the compositor participates in pacing the capture and
  client callback flow before frames reach the viewer.
- Update Application-window requirements so runtime pacing controls are exposed alongside the
  existing management/performance surfaces and follow the existing target-FPS contract.
- Preserve `target_fps = 0` as the uncapped mode and keep the existing viewer fallback behavior
  explicit when present-wait-style pacing is unavailable.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `render-pipeline`: `render.target_fps` changes from viewer-final-present pacing to a global
  end-to-end pacing contract that still reuses the current present-wait/fallback subsystem.
- `compositor-capture`: the compositor capture path changes from immediate callback-driven flow to a
  paced capture/publication path that participates in the global target FPS contract.
- `app-window`: the Application window changes from metrics-only pacing visibility to runtime pacing
  control plus explicit precedence with existing config/CLI target-FPS settings.

## Non-goals

- Introduce separate app-side, compositor-side, or viewer-side FPS targets in v1.
- Accept a viewer-only solution that leaves the nested compositor uncapped.
- Add a second numeric jitter gate in v1 beyond the FPS tolerance rule.
- Redesign unrelated shader, surface-management, or performance-panel behavior.
- Change packaging, dependency, or distribution workflows.

## Impact

- Affected modules: `src/compositor`, `src/render/backend`, `src/app`, `src/ui`, and `src/util`.
- Likely affected files:
  - `src/compositor/compositor_server.hpp`
  - `src/compositor/compositor_server.cpp`
  - `src/compositor/compositor_xdg.cpp`
  - `src/compositor/compositor_xwayland.cpp`
  - `src/compositor/compositor_layer_shell.cpp`
  - `src/compositor/compositor_input.cpp`
  - `src/compositor/compositor_present.cpp`
  - `src/compositor/compositor_state.hpp`
  - `src/render/backend/render_output.cpp`
  - `src/render/backend/render_output.hpp`
  - `src/render/backend/vulkan_backend.cpp`
  - `src/app/application.cpp`
  - `src/app/application.hpp`
  - `src/app/main.cpp`
  - `src/ui/imgui_layer.cpp`
  - `src/ui/imgui_layer.hpp`
  - `src/util/config.hpp`
  - `src/util/config.cpp`
- Impacted OpenSpec specs:
  - `openspec/specs/render-pipeline/spec.md`
  - `openspec/specs/compositor-capture/spec.md`
  - `openspec/specs/app-window/spec.md`
- No expected packaging or dependency changes.

## Risks

- Compositor pacing can drift from viewer pacing if the shared target is not routed through one
  policy boundary.
- Host-specific behavior can diverge between Wayland and X11 if callback pacing semantics are not
  made explicit in the contract.
- Runtime controls can drift from config/CLI semantics if the precedence chain is not defined.
- Injecting pacing into the compositor event loop can regress responsiveness if the contract does not
  bound what is paced vs what remains immediate.

## Validation Plan

Verification contract:
- Baseline gates:
  - `pixi run build -p debug`
  - `pixi run build -p asan`
  - `pixi run build -p quality`
- Environment-agnostic automated checks:
  - `pixi run test -p asan`
- Environment-sensitive checks:
  - `pixi run test -p test` when local compositor/runtime support is available
  - targeted host validation for Wayland and X11 pacing behavior when the local runtime supports both
- Manual fallback:
  - allowed only for visible pacing verification on Wayland/X11 hosts and Application-window runtime
    control behavior when automated coverage is unavailable
  - record target FPS, host type, observed FPS window, and proof location
- Mandatory checks with no fallback:
  - the baseline build/static-analysis gates above
- Pass criteria:
  - one global pacing target governs the compositor and viewer present workflow
  - `target_fps = 0` remains uncapped
  - Wayland-hosted and X11-hosted sessions stay within +/-3 FPS of the selected target over 10
    seconds
  - the Application window exposes runtime pacing control without breaking the existing config/CLI
    contract
