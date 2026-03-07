## 1. Proposal Contract Lock and Compile-Safe Baseline

- [x] 1.1 Confirm the proposal/design/spec artifacts remain the source of truth for module boundaries, migration order, behavior-preservation scope, and divergence handling before touching product code.
- [x] 1.2 Map the current `src/compositor/compositor_server.cpp` responsibilities to concrete extraction targets (`xdg`, `xwayland`, `input`, `focus`, `cursor`, `present`, `layer_shell`, `core`) and record any shared state or quirks that must remain centralized.
- [x] 1.3 Create the compile-safe declaration seam before behavior extraction:
  - `src/compositor/compositor_state.hpp` for the central compositor state declaration and only the shared POD/state members stored on that object.
  - `src/compositor/compositor_targets.hpp` for `InputTarget` plus cursor/surface-coordinate helper declarations shared by input/focus/cursor/present.
  - `src/compositor/compositor_protocol_hooks.hpp` for `XdgToplevelHooks`, `XdgPopupHooks`, `XWaylandSurfaceHooks`, `LayerSurfaceHooks`, and `ConstraintHooks` listener structs.
- [x] 1.4 Update `src/compositor/CMakeLists.txt` as each new compositor translation unit is introduced so the build graph stays compile-complete throughout extraction.

## 2. XDG and XWayland Lifecycle Extraction

- [x] 2.1 Extract XDG toplevel and popup hook structs plus lifecycle handlers into `src/compositor/compositor_xdg.*`, preserving current listener registration, mapping, commit, and destroy behavior.
- [x] 2.2 Extract XWayland setup and lifecycle handlers into `src/compositor/compositor_xwayland.*`, preserving association/map/commit/destroy behavior and X11-specific workarounds/comments.
- [x] 2.3 Keep subsystem code operating on one central compositor implementation state object rather than duplicating ownership across extracted lifecycle modules.
- [x] 2.4 Keep XDG popup hook ownership authoritative in `src/compositor/compositor_xdg.*`, including popup hooks created from layer-shell `new_popup` forwarding.

## 3. Input and Focus Separation

- [x] 3.1 Extract input queue injection, compositor-thread event draining, and keyboard/pointer/button/axis dispatch into `src/compositor/compositor_input.*`.
- [x] 3.2 Extract input target resolution, focus switching, hit-testing, pointer-constraint activation/deactivation, confinement, cursor-hint application, and cursor-local coordinate helpers into `src/compositor/compositor_focus.*`.
- [x] 3.3 Preserve focus wakeups, resize request wakeups, auto/manual targeting behavior, pointer-confine/pointer-lock behavior, and layer/XWayland interactions while separating input flow from target resolution.

## 4. Layer Shell Extraction

- [x] 4.1 Extract layer-shell hook structs, lifecycle handlers, popup forwarding, keyboard-interactivity handling, and render-order integration into `src/compositor/compositor_layer_shell.*` after XDG popup ownership is isolated.
- [x] 4.2 Preserve layer-shell render ordering, popup routing into the XDG popup path, and exclusive keyboard-focus restoration behavior during extraction.

## 5. Cursor and Presentation Extraction

- [x] 5.1 Extract cursor theme setup, fallback cursor generation, cursor frame lookup, visibility/reset/hint/update, and overlay rendering into `src/compositor/compositor_cursor.*`.
- [x] 5.2 Extract presented-frame tracking, retained-frame refresh/reset, render-to-frame flow, and DMA-BUF export logic into `src/compositor/compositor_present.*`.
- [x] 5.3 Keep cursor and presentation modules wired through the same central compositor state object and preserve existing cursor/focus/presentation interactions.

## 6. Core Consolidation and Facade Reduction

- [x] 6.1 Consolidate wlroots bootstrap, backend/output/event loop setup, compositor thread lifecycle, and teardown into `src/compositor/compositor_core.*`.
- [x] 6.2 Keep `src/compositor/compositor_core.*` limited to startup, shutdown, backend/output/event-loop orchestration, compositor thread lifecycle, and teardown; assign every other extracted helper to a concrete subsystem or subsystem-owned narrow header.
- [x] 6.3 Reduce `src/compositor/compositor_server.cpp` and `src/compositor/compositor_server.hpp` to facade/public API responsibilities and high-level delegation only.

## 7. Structural Verification and Behavior Preservation

- [x] 7.1 Verify the structural acceptance criteria:
  - `src/compositor/compositor_server.cpp` no longer contains all compositor subsystems.
  - The extracted subsystem files exist and match the approved boundaries or documented close variants.
  - `src/compositor/CMakeLists.txt` lists each new compositor translation unit.
  - `src/compositor/compositor_core.*` contains only startup, shutdown, backend/output/event-loop orchestration, compositor thread lifecycle, and teardown responsibilities.
  - No compositor `misc/helpers/utils` dumping-ground files were introduced.
- [x] 7.2 Run project-aligned build/test/static commands:
  - `pixi run build -p debug`
  - `pixi run build -p asan`
  - `pixi run test -p asan` when the full ASAN suite is supported by the local runtime environment
  - `pixi run build -p quality`
  - `ctest --preset asan -R "goggles_tests|goggles_test_child_death_signal|goggles_test_headless_child_exit" --output-on-failure`
- [x] 7.3 Run environment-sensitive compositor integration checks when supported:
  - `ctest --preset asan -R "goggles_auto_input_forwarding_(x11|wayland)" --output-on-failure`
  - `ctest --preset asan -R "goggles_headless_integration(_png_exists)?" --output-on-failure` whenever `src/compositor/compositor_present.*` or DMA-BUF export code is touched; this check is mandatory and has no manual fallback
- [x] 7.4 When `goggles_auto_input_forwarding_x11` or `goggles_auto_input_forwarding_wayland` cannot run because automated CTest coverage is unavailable but an equivalent interactive runtime exists, record fallback manual evidence from the repository root with exact prerequisites, observations, and proof:
  - prerequisite: active compositor-compatible X11 runtime for `./build/asan/tests/goggles_manual_input_x11`
  - prerequisite: active compositor-compatible Wayland runtime with `WAYLAND_DISPLAY` available for `./build/asan/tests/goggles_manual_input_wayland`
  - prerequisite: active compositor-compatible X11 runtime for `./build/asan/tests/goggles_manual_surface_selector_x11`
  - prerequisite: active compositor-compatible Wayland runtime with `WAYLAND_DISPLAY` available for `./build/asan/tests/goggles_manual_surface_selector_wayland`
  - record which automated check was skipped, why it was unavailable, what was observed, and where the evidence was stored
- [x] 7.5 Record a compositor behavior-preservation checklist covering:
  - startup/shutdown order
  - Wayland client handling
  - XWayland client handling
  - input forwarding
  - focus targeting
  - pointer constraints (activation, confine, cursor hints)
  - layer-shell render ordering, popup routing, and exclusive keyboard focus
  - cursor visibility/behavior
  - presented-frame acquisition/export
- [x] 7.6 Run quirk-preservation checks and record the proving file for each one:
  - XWayland re-activation before each key/pointer event
  - the no-destroy-listener rule on `xsurface->surface`
  - stable allocation for XDG, XWayland, and layer-surface hook containers
  - layer-shell `new_popup` forwarding into XDG-owned popup hooks
- [x] 7.7 Run static spot checks for scope drift and policy-sensitive regressions in touched compositor files with explicit expectations:
  - `grep -R --line-number '\bthrow\b' src/compositor --include='*.cpp' --include='*.hpp'`
    - expected result: no new expected-failure exception paths in touched files
  - `grep -R --line-number 'std::thread\|std::jthread' src/compositor --include='*.cpp' --include='*.hpp'`
    - expected result: relocation of the existing compositor thread primitive into `compositor_core.*` is allowed, but touched files introduce no net-new thread owners, no additional thread primitives, and no new `std::thread` usage
  - `grep -R --line-number 'Vk[A-Za-z0-9_]*' src/compositor --include='*.cpp' --include='*.hpp'`
    - expected result: no new raw `Vk*` usage appears in touched compositor sources or headers

## 8. Spec/Design Consistency and Apply Handoff

- [x] 8.1 Confirm implementation matches `openspec/changes/refactor-compositor-server-modules/specs/compositor-module-layout/spec.md` and `openspec/changes/refactor-compositor-server-modules/design.md`.
- [x] 8.2 If extraction requires behavior divergence or a materially different module boundary, stop apply work and reconcile proposal/spec/design artifacts before continuing.
- [x] 8.3 Prepare `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md` listing touched compositor files, preserved quirks/workarounds, any documented close-variant boundaries, exact verification results, skipped checks, fallback evidence locations, and the file proving each quirk remained adjacent to its subsystem.

## 9. Requirement Traceability

- [x] 9.1 Keep this traceability matrix current during apply so every requirement/scenario maps to implementation tasks and verification evidence.

| Requirement / Scenario | Task IDs | Verification commands / evidence |
| --- | --- | --- |
| Compositor Server Facade Remains Stable / Public facade preserved after split | 6.1, 6.3, 7.1, 8.3 | 7.1 checklist item citing `src/compositor/compositor_server.cpp`, `src/compositor/compositor_server.hpp`, and `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md`; `pixi run build -p debug` |
| Single Compositor State Authority / Ownership remains centralized after extraction | 1.2, 1.3, 2.3, 5.3, 6.1, 8.3 | 7.1 checklist item citing `src/compositor/compositor_state.hpp` and extracted subsystem files in `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md`; `pixi run build -p quality` |
| Responsibility-Oriented Compositor Modules / Localized edit surface for protocol lifecycle | 1.2, 1.3, 2.2, 6.2, 7.1, 8.3 | 7.1 checklist item citing `src/compositor/compositor_xwayland.*`, `src/compositor/compositor_core.*`, and `src/compositor/CMakeLists.txt` in `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md`; `pixi run build -p debug` |
| Responsibility-Oriented Compositor Modules / Localized edit surface for input targeting | 1.2, 1.3, 3.1, 3.2, 6.2, 7.1, 8.3 | 7.1 checklist item citing `src/compositor/compositor_input.*`, `src/compositor/compositor_focus.*`, and `src/compositor/compositor_targets.hpp` in `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md`; `pixi run build -p debug` |
| Extraction Contract Is Explicit for Apply / Migration order is explicit for implementation | 1.1, 1.3, 1.4, 2.1, 2.2, 3.1, 3.2, 4.1, 5.1, 5.2, 6.1, 7.1, 7.2, 8.2 | 7.1 checklist item citing `src/compositor/CMakeLists.txt` plus the ordered subsystem files in `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md`; `pixi run build -p asan`; `pixi run build -p quality`; `ctest --preset asan -R "goggles_tests|goggles_test_child_death_signal|goggles_test_headless_child_exit" --output-on-failure` |
| Extraction Contract Is Explicit for Apply / Behavior preservation is verified explicitly | 7.2, 7.3, 7.4, 7.5, 8.3 | `pixi run test -p asan` when supported; `ctest --preset asan -R "goggles_auto_input_forwarding_(x11|wayland)" --output-on-failure`; `ctest --preset asan -R "goggles_headless_integration(_png_exists)?" --output-on-failure`; fallback notes in `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md` |
| Extraction Contract Is Explicit for Apply / Input-routing fallback is explicit and bounded | 7.3, 7.4, 8.3 | repo-root manual fallback commands from 7.4 plus prerequisites, observations, and proof recorded in `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md` |
| Extraction Contract Is Explicit for Apply / Presentation and export verification stays mandatory | 5.1, 5.2, 7.3, 8.3 | `ctest --preset asan -R "goggles_headless_integration(_png_exists)?" --output-on-failure`; mandatory-check note recorded in `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md` |
| Behavior Is Preserved Across the Module Split / Protocol and input behavior remain unchanged | 2.1, 2.2, 3.1, 3.2, 4.1, 4.2, 7.3, 7.4, 7.5, 8.3 | `ctest --preset asan -R "goggles_auto_input_forwarding_(x11|wayland)" --output-on-failure`; repo-root manual fallback commands from 7.4 when allowed; preserved-behavior checklist and observations in `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md` |
| Behavior Is Preserved Across the Module Split / Presentation and export behavior remain unchanged | 5.1, 5.2, 7.3, 7.5, 8.3 | `ctest --preset asan -R "goggles_headless_integration(_png_exists)?" --output-on-failure`; preserved-behavior checklist and results in `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md` |
| Behavior-Critical Quirks Stay With Their Subsystems / XWayland quirks remain isolated and documented | 1.2, 2.2, 3.2, 4.1, 7.6, 8.3 | 7.6 checklist with proving files for XWayland, XDG, and layer-shell hook ownership plus `openspec/changes/refactor-compositor-server-modules/implementation-handoff.md` |
