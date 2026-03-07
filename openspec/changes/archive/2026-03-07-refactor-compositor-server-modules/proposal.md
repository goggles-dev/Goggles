## Why

`src/compositor/compositor_server.cpp` currently concentrates wlroots bootstrap, XDG/XWayland/layer-shell lifecycle, input dispatch, focus targeting, cursor handling, frame presentation/export, pointer constraints, and teardown in one implementation unit. That concentration makes future edits expensive because a localized change requires loading unrelated compositor domains and increases the risk of cross-domain drift.

## Problem

- The nested compositor implementation is too large and responsibility-mixed for safe, local editing.
- Protocol lifecycle, input routing, focus logic, cursor behavior, layer-shell behavior, pointer-constraint behavior, and presentation/export logic currently share one translation unit, so narrowly scoped changes still require broad context loading.
- Important compositor quirks and wlroots/XWayland workarounds are harder to audit because subsystem logic is not isolated near the behavior it constrains.

## Scope

This change is a behavior-preserving internal refactor of `src/compositor/` that introduces a stable, responsibility-oriented module layout for the compositor implementation.

- Keep `CompositorServer` public API stable except for a very small internal-only adjustment if extraction requires it.
- Split the implementation into subsystem-oriented files centered on: facade, core lifecycle, input, focus, cursor, presentation/export, XDG lifecycle, XWayland lifecycle, and layer-shell lifecycle.
- Preserve one central compositor implementation state object (or equivalent single source of truth) so ownership and cross-thread state remain easy to audit.
- Preserve existing startup/shutdown order, protocol behavior, input behavior, focus behavior, layer-shell behavior, cursor behavior, pointer-constraint behavior, presented-frame/export behavior, and documented quirks/workarounds.
- Follow migration order that first establishes a compile-safe internal declaration seam and updates `src/compositor/CMakeLists.txt` incrementally as each new translation unit lands, then extracts XDG, XWayland, input/focus (including pointer constraints), layer-shell, cursor/presentation, and finally bootstrap/teardown consolidation unless a smaller compile-safety step is required.

## Non-goals

- Redesigning the compositor architecture beyond file/module decomposition.
- Replacing wl_listener patterns with a new framework except for a very small local cleanup required to complete extraction safely.
- Introducing a generic `misc`, `helpers`, or `utils` dumping-ground module.
- Over-fragmenting event flows into many micro-files.
- Changing protocol semantics, rendering semantics, or resource ownership unless required for safe extraction and explicitly documented.

## What Changes

- Reduce `compositor_server.hpp` / `compositor_server.cpp` to facade and public API responsibilities, with high-level create/start/stop delegation only.
- Introduce stable compositor implementation modules close to these target boundaries:
  - `compositor_core.*`: wlroots bootstrap, backend/output/event loop setup, compositor thread lifecycle, teardown.
  - `compositor_input.*`: event queue injection, compositor-thread input processing, keyboard/pointer/button/axis dispatch, resize/focus wakeups.
  - `compositor_focus.*`: target resolution, focus switching, hit-testing, pointer-constraint activation/deactivation, confinement, cursor-hint application, and cursor-local coordinate helpers tied to targeting.
  - `compositor_cursor.*`: theme setup, fallback cursor generation, cursor frame lookup, visibility/reset/hint/update/render overlay.
  - `compositor_present.*`: presented-frame tracking, refresh/reset/export, render-to-frame path and related presentation logic.
  - `compositor_xdg.*`: XDG toplevel/popup lifecycle hooks and handlers.
  - `compositor_xwayland.*`: XWayland setup plus associate/map/commit/destroy handling and X11-specific quirks.
-  - `compositor_layer_shell.*`: layer-shell hooks, lifecycle, render-order integration, and forwarding into XDG-owned popup hook lifecycle handling.
- Define narrowly scoped internal headers up front so implementation work does not invent a broad internal catch-all header during extraction.
- Keep important subsystem comments and workarounds next to the logic they constrain, especially XWayland activation quirks, destroy-listener restrictions, stable hook allocation requirements, and layer-shell popup forwarding rules.
- Add implementation-facing design and task traceability so the change can be executed from the repository artifacts alone.

## Capabilities

### New Capabilities
- `compositor-module-layout`: Responsibility-oriented compositor module boundaries that preserve current nested compositor behavior while improving editing locality and state auditability.

### Modified Capabilities
- None.

## Risks

- Extraction could accidentally change startup/shutdown sequencing or listener registration order.
- Shared state could fragment if module boundaries duplicate ownership instead of delegating through one implementation state object.
- Input/focus/cursor/pointer-constraint flows have tightly coupled behavior; an overly mechanical split could break routing or hide important quirks.
- Layer-shell routing could regress if popup forwarding or exclusive keyboard-focus handling drifts during extraction.
- Presentation/export paths could regress if render ordering, retained-frame logic, or cursor overlay integration drift during extraction.

## Validation Plan

- Structural verification:
  - Confirm `src/compositor/compositor_server.cpp` is reduced to facade/high-level delegation and no longer contains all compositor subsystems.
  - Confirm the new subsystem files exist and align with the approved module boundaries or documented close variants.
  - Confirm `src/compositor/CMakeLists.txt` lists each new compositor translation unit.
  - Confirm `compositor_core.*` only owns startup, shutdown, backend/output/event-loop orchestration, compositor thread lifecycle, and teardown responsibilities.
- Build and static verification:
  - `pixi run build -p debug`
  - `pixi run build -p asan`
  - `pixi run build -p quality`
  - `ctest --preset asan -R "goggles_tests|goggles_test_child_death_signal|goggles_test_headless_child_exit" --output-on-failure`
  - `pixi run test -p asan` when the full ASAN suite is supported by the local runtime environment
  - `ctest --preset asan -R "goggles_auto_input_forwarding_(x11|wayland)" --output-on-failure`
  - `ctest --preset asan -R "goggles_headless_integration(_png_exists)?" --output-on-failure`
- Behavioral preservation checklist:
  - Compositor startup/shutdown order remains unchanged.
  - Wayland client handling remains unchanged for XDG toplevels and popups.
  - XWayland client handling remains unchanged, including X11-specific quirks/workarounds.
  - Input forwarding and event wakeups remain unchanged.
  - Focus targeting and hit-testing remain unchanged.
  - Pointer-constraint activation, confinement, and cursor-hint behavior remain unchanged.
  - Layer-shell render ordering, popup routing, and exclusive keyboard-interactivity behavior remain unchanged.
  - Cursor visibility, positioning, lock/confine, and overlay rendering remain unchanged.
  - Presented frame acquisition, retained-frame behavior, and DMA-BUF export remain unchanged.
  - When `goggles_auto_input_forwarding_x11` or `goggles_auto_input_forwarding_wayland` cannot run but an equivalent interactive runtime exists, capture fallback evidence and record the prerequisites, observations, and stored proof using:
    - `./build/asan/tests/goggles_manual_input_x11`
    - `./build/asan/tests/goggles_manual_input_wayland`
    - `./build/asan/tests/goggles_manual_surface_selector_x11`
    - `./build/asan/tests/goggles_manual_surface_selector_wayland`
  - When presented-frame or DMA-BUF export code is touched, `goggles_headless_integration*` remains mandatory; do not replace it with manual fallback.

## Divergence Handling

- If extraction uncovers a required behavior change, implementation MUST stop and reconcile proposal/spec/design artifacts before implementation continues.
- If a target module boundary proves unsafe, implementation MAY use a documented close variant only when it preserves responsibility-oriented locality, keeps a single state authority, and records the rationale in the implementation handoff.

## Impact

- **Code modules/files**: `src/compositor/compositor_server.hpp`, `src/compositor/compositor_server.cpp`, `src/compositor/CMakeLists.txt`, plus new `src/compositor/compositor_*.hpp/.cpp` internal modules.
- **Runtime systems**: wlroots bootstrap, XDG shell lifecycle, XWayland lifecycle, layer-shell handling, pointer constraints, input forwarding, focus resolution, cursor rendering, and compositor-presented frame export.
- **Public API**: `CompositorServer` surface is expected to remain stable.
- **Tests/verification**: compositor-focused unit/integration coverage and preset-driven build/test/static checks, with explicit fallback manual evidence when environment-sensitive tests cannot run.
- **OpenSpec artifacts impacted**:
  - Delta spec introduced by this change: `openspec/changes/refactor-compositor-server-modules/specs/compositor-module-layout/spec.md`
  - Proposed living-spec sync target after archive/sync: `openspec/specs/compositor-module-layout/spec.md`
- **Policy-sensitive areas**:
  - Error handling and logging boundaries MUST remain unchanged unless explicitly required.
  - Ownership/lifetime MUST remain centralized and RAII-safe.
  - Render/presentation behavior MUST preserve existing threading constraints and resource ownership rules.
