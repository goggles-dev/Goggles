## Context

The nested compositor in `src/compositor/` is currently centered on `compositor_server.cpp` and `compositor_server.hpp`. The implementation mixes wlroots bootstrap, compositor thread lifecycle, XDG/XWayland/layer-shell listeners, input queue draining, hit-testing and focus switching, pointer constraints, cursor state/rendering, frame presentation/export, and teardown in one large translation unit.

This is a brownfield maintainability change, not a behavior redesign. The proposal requires a responsibility-oriented split that improves editing locality while preserving compositor startup/shutdown order, protocol semantics, input routing, focus behavior, layer-shell behavior, pointer-constraint behavior, cursor behavior, presented-frame behavior, export behavior, and existing workarounds.

Current code characteristics that shape the design:

- `CompositorServer` already acts as the public entrypoint used outside `src/compositor/`.
- An internal `Impl` object currently centralizes wlroots handles, hook storage, synchronization primitives, queues, presentation state, focus/cursor metadata, and pointer-constraint state.
- Several behaviors depend on localized quirks and comments, especially around XWayland activation, destroy-listener constraints, stable hook allocation, and layer-shell popup forwarding into XDG popup handling.
- Input, focus, pointer constraints, cursor, and presentation paths are coupled through shared state and compositor-thread ordering, so a purely mechanical extraction is risky.

Policy constraints from `docs/project_policies.md` apply, along with the existing compositor-local runtime conventions already present in `src/compositor/`:

- Expected runtime failures MUST keep `Result`-style propagation and MUST NOT switch to exception-based handling.
- Ownership/lifetime MUST remain RAII-safe and easy to audit.
- Build/test workflows MUST use Pixi tasks and CMake/CTest presets.
- The compositor event loop MUST remain non-blocking with thread coordination consistent with current queue/eventfd patterns.

## Goals / Non-Goals

**Goals:**

- Reduce `compositor_server.cpp` to facade/high-level orchestration.
- Split compositor implementation into a small set of subsystem-oriented modules with stable, self-describing responsibilities.
- Preserve one central implementation state object as the compositor single source of truth.
- Keep cross-cutting quirks and constraints near the subsystem logic they govern.
- Provide an implementation plan and verification map that can be executed from the repository artifacts alone.

**Non-Goals:**

- Changing external `CompositorServer` behavior or protocol semantics.
- Replacing wl_listener-based lifecycle wiring with a new event framework.
- Introducing generic utility buckets divorced from subsystem ownership.
- Splitting code so aggressively that one event flow is scattered across many tiny files.
- Reworking compositor ownership, threading, rendering, or protocol models beyond what safe extraction requires.

## Decisions

### 1) Keep `CompositorServer` as the only public facade

Decision:

- `compositor_server.hpp` remains the public API surface.
- `compositor_server.cpp` keeps factory/create/start/stop/public method entrypoints and delegates subsystem work through internal module functions.
- Any signature change MUST be internal-only and minimal.

Rationale:

- Preserves external integration points and keeps the public surface easy to reason about.
- Gives future editors a stable top-level entrypoint without mixing protocol or rendering details into the facade.

Alternatives considered:

- Promote subsystem headers into new semi-public API: rejected because it broadens the integration surface and weakens locality.
- Split public API by protocol domain: rejected because external callers still conceptually use one compositor service.

### 2) Preserve one central compositor state authority

Decision:

- Keep one internal implementation state object (`Impl` or an equivalent renamed type) that owns wlroots objects, listener storage, event queues, focus/cursor state, pointer-constraint state, and presented-frame/export state.
- Subsystem files operate on that central state by reference/pointer; they MUST NOT duplicate ownership of compositor-global resources.

Rationale:

- Existing behavior relies on tightly shared state across protocol lifecycle, input dispatch, focus targeting, pointer constraints, cursor handling, and presentation.
- A single state authority keeps teardown ordering, synchronization, and ownership audits tractable.

Alternatives considered:

- Per-subsystem owner objects with distributed lifetime: rejected because it increases teardown complexity and risks behavior drift.
- A broad service-locator internal API: rejected because it hides ownership rather than clarifying it.

### 3) Module boundaries follow responsibilities, not helper extraction

Decision:

- Extract subsystem modules aligned to behavior domains:
  - `compositor_core.*`
  - `compositor_input.*`
  - `compositor_focus.*`
  - `compositor_cursor.*`
  - `compositor_present.*`
  - `compositor_xdg.*`
  - `compositor_xwayland.*`
  - `compositor_layer_shell.*`
- Shared declarations are permitted only through narrow internal headers needed by a specific subsystem boundary.
- Helpers meaningful only inside one subsystem stay in that subsystem file.
- `compositor_focus.*` owns pointer-constraint listener lifecycle, activation/deactivation, confinement, and cursor-hint application; `compositor_input.*` only consults that state when dispatching motion/button/axis events.
- `compositor_layer_shell.*` owns layer-surface lifecycle and render-order integration, while `compositor_xdg.*` remains the sole owner of XDG popup hook creation/destruction, including popups forwarded from layer-shell surfaces.
- `compositor_core.*` MUST stay limited to startup, shutdown, backend/output/event-loop orchestration, compositor thread lifecycle, and teardown; extracted helpers that do not fit that scope MUST belong to a concrete subsystem or a narrow internal header owned by that subsystem.

Rationale:

- Responsibility-oriented files minimize context bloat for future edits.
- Future maintenance benefits when one task maps to one conceptual file instead of requiring a giant mixed-context unit.

Alternatives considered:

- Split by helper category (`listeners`, `render_helpers`, `state_utils`): rejected because it still spreads one behavior across many files.
- Extract only protocol-specific code and leave the rest in `compositor_server.cpp`: rejected because input/focus/cursor/presentation would remain context-heavy.

### 4) Establish a compile-safe declaration seam before behavior extraction

Decision:

- Create `src/compositor/compositor_state.hpp` for the central compositor state declaration and only the shared POD/state members stored on that object.
- Create `src/compositor/compositor_targets.hpp` for `InputTarget` plus cursor/surface-coordinate helper declarations shared by input/focus/cursor/present.
- Create `src/compositor/compositor_protocol_hooks.hpp` for `XdgToplevelHooks`, `XdgPopupHooks`, `XWaylandSurfaceHooks`, `LayerSurfaceHooks`, and `ConstraintHooks` listener structs.
- Update `src/compositor/CMakeLists.txt` as each new compositor translation unit is introduced so the build graph stays compile-complete throughout extraction.

Rationale:

- The current `Impl` declaration block is too monolithic for safe early extraction of XDG/XWayland without first moving shared declarations into narrow internal headers.
- Naming the exact internal headers prevents implementation from inventing a broad catch-all internal header.

Alternatives considered:

- Defer header work until after the first source split: rejected because the early extraction waves would not be compile-safe.
- Use one `compositor_internal.hpp` catch-all header: rejected because it recreates the context-bloat problem in header form.

### 5) Extract in risk-ordered migration waves

Decision:

- Follow this migration order:
  0. Establish the compile-safe declaration seam and update `src/compositor/CMakeLists.txt`.
  1. XDG lifecycle.
  2. XWayland lifecycle.
  3. Input dispatch and focus/hit-test split, including pointer constraints.
  4. Layer-shell lifecycle/render integration after XDG popup ownership is isolated.
  5. Cursor and presentation/export paths.
  6. Core bootstrap/teardown consolidation and final facade reduction.

Rationale:

- The declaration seam prevents phase-1/phase-2 extraction from failing to compile once method/type declarations leave the monolithic `Impl` block.
- XDG and XWayland lifecycle blocks are large, self-identifying seams with clear listener ownership.
- Input/focus can be split after protocol handlers are isolated, reducing concurrent churn and clarifying pointer-constraint ownership.
- Layer-shell extraction becomes safer once XDG popup ownership is explicit and input/focus seams are stable.
- Cursor and presentation depend on stabilized focus/targeting behavior.
- Core consolidation last avoids premature churn in wiring while subsystem extraction is still moving.

Alternatives considered:

- Start with bootstrap/teardown first: rejected because it risks destabilizing all later extractions.
- Extract cursor/presentation before input/focus: rejected because those paths depend on stable targeting semantics.

### 6) Preserve behavior through subsystem-local quirk retention

Decision:

- Existing comments that capture non-obvious constraints MUST move with the subsystem logic they explain.
- XWayland-specific re-activation and destroy-listener restrictions stay in `compositor_xwayland.*`.
- Stable hook allocation notes stay with the hook-owning subsystem (`compositor_xdg.*`, `compositor_xwayland.*`, `compositor_layer_shell.*`).
- Pointer-constraint confinement/cursor-hint rationale stays in `compositor_focus.*`.
- Layer-shell popup forwarding rationale stays split as one explicit ownership rule: `compositor_layer_shell.*` forwards `new_popup` events, and `compositor_xdg.*` owns the popup hook lifecycle created from those events.

Rationale:

- These comments explain behavior-critical constraints, not optional narration.
- Separating the quirk from the code increases the chance of accidental regression in future edits.

Alternatives considered:

- Consolidate quirks into one internal notes header: rejected because it disconnects rationale from behavior.

### 7) Verification is explicit and behavior-preserving

Decision:

- Implementation tasks MUST include both structural checks (module boundaries, facade reduction, central state retention, `CMakeLists.txt` coverage, and `compositor_core.*` scope) and runtime-oriented checks (build/test/static commands plus a behavior checklist).
- Verification remains preset-driven and project-policy aligned.

Verification evidence is expected to include:

- `ctest --preset asan -R "goggles_tests|goggles_test_child_death_signal|goggles_test_headless_child_exit" --output-on-failure` for environment-agnostic automated coverage prepared by the ASAN build tree.
- `pixi run test -p asan` when the full ASAN suite is supported by the local runtime environment.
- `ctest --preset asan -R "goggles_auto_input_forwarding_(x11|wayland)" --output-on-failure` for XWayland/Wayland input routing when the environment supports those compositor tests.
- `ctest --preset asan -R "goggles_headless_integration(_png_exists)?" --output-on-failure` whenever presented-frame or DMA-BUF export code is touched; completion is blocked until an environment capable of running that check is available.
- Manual fallback evidence for `goggles_auto_input_forwarding_x11` / `goggles_auto_input_forwarding_wayland` only when automated CTest coverage is unavailable but an equivalent interactive runtime exists, recorded alongside prerequisites, observations, and stored proof, using repo-root commands:
  - `./build/asan/tests/goggles_manual_input_x11`
  - `./build/asan/tests/goggles_manual_input_wayland`
  - `./build/asan/tests/goggles_manual_surface_selector_x11`
  - `./build/asan/tests/goggles_manual_surface_selector_wayland`
- No manual fallback for `goggles_headless_integration*` when presented-frame or DMA-BUF export code is touched.
- Quirk-level checks for XWayland re-activation before each input event, the no-destroy-listener rule for `xsurface->surface`, stable hook allocation for XDG/XWayland/layer-shell hook containers, and layer-shell-to-XDG popup forwarding.

Rationale:

- This refactor is successful only if locality improves without behavior drift.
- Implementation needs concrete verification evidence, not an implied “no behavior change” claim.

Alternatives considered:

- Rely only on compile/test gates: rejected because structural acceptance criteria and quirk preservation would remain under-specified.

## Risks / Trade-offs

- [Risk] Input/focus/cursor/pointer-constraint logic remains too entangled for clean boundaries -> Mitigation: keep focus resolution and pointer constraints in `compositor_focus.*`, keep input dispatch in `compositor_input.*`, and allow narrow internal declarations rather than generic helpers.
- [Risk] Extraction changes listener registration or teardown order -> Mitigation: preserve one state authority, document ordering decisions, and verify startup/shutdown plus protocol behavior explicitly.
- [Risk] `compositor_core.*` becomes a second dumping ground -> Mitigation: keep it limited to bootstrap, thread lifecycle, backend/output/event-loop orchestration, and teardown only.
- [Risk] Layer-shell/XDG popup ownership drifts across two modules -> Mitigation: enforce one popup ownership rule (`compositor_xdg.*`) and keep layer-shell at event-forwarding/lifecycle/render responsibilities only.
- [Risk] A module boundary proves slightly different in code than planned -> Mitigation: allow a documented close variant only when responsibility ownership stays clear and external behavior remains unchanged.
- [Trade-off] Keeping one central state object retains some coupling -> Mitigation: accept this coupling to preserve behavior and ownership clarity while still improving file-level locality.

## Migration Plan

1. Introduce `compositor_state.hpp`, `compositor_targets.hpp`, and `compositor_protocol_hooks.hpp`, and move only the declarations needed to make multi-file extraction compile-safe.
2. Update `src/compositor/CMakeLists.txt` to add each new compositor translation unit as it lands.
3. Extract XDG setup, toplevel handlers, and popup handlers into `compositor_xdg.*`.
4. Extract XWayland setup and lifecycle handlers into `compositor_xwayland.*`, preserving X11-specific quirks and comments in the new subsystem.
5. Separate input queue injection/dispatch into `compositor_input.*` and target resolution/focus/pointer-constraint behavior into `compositor_focus.*`.
6. Extract layer-shell lifecycle/render-order integration into `compositor_layer_shell.*`, with popup events forwarded to XDG-owned popup hook creation.
7. Extract cursor setup/update/render logic and presented-frame/export logic into `compositor_cursor.*` and `compositor_present.*`.
8. Consolidate bootstrap, backend/output setup, compositor-thread lifecycle, and teardown into `compositor_core.*`, leaving `compositor_server.cpp` as facade/orchestration only.
9. Run structural and behavioral verification commands/checklists after the split.
10. If implementation uncovers a required behavior change, stop and reconcile proposal/spec/design artifacts before continuing.

Rollback strategy:

- Revert the module extraction while restoring the previous single-file implementation shape if the split causes behavior regressions.
- Do not keep a partially duplicated ownership model during rollback.

## Open Questions

- Whether additional targeted compositor tests should be added if the existing `goggles_auto_input_forwarding_*` and `goggles_headless_integration*` coverage proves insufficient for presentation/export preservation.
