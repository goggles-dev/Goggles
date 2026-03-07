## ADDED Requirements

### Requirement: Compositor Server Facade Remains Stable
The compositor implementation SHALL preserve `CompositorServer` as the public integration facade while moving subsystem logic out of `compositor_server.cpp`.

The refactor SHALL:

- Keep `compositor_server.hpp` as the public API declaration surface.
- Keep `compositor_server.cpp` limited to public method entrypoints and high-level delegation.
- Preserve existing `CompositorServer` externally observable behavior unless a very small internal-only adjustment is explicitly documented in the change artifacts.

#### Scenario: Public facade preserved after split
- **GIVEN** the compositor refactor is complete
- **WHEN** external callers integrate the nested compositor
- **THEN** external callers still integrate through `CompositorServer`
- **AND** compositor subsystem implementations no longer remain concentrated in one giant `compositor_server.cpp`

### Requirement: Single Compositor State Authority
The compositor implementation SHALL retain one central implementation state object as the single source of truth for wlroots resources, synchronization primitives, listener storage, focus state, cursor state, pointer-constraint state, and presented-frame/export state.

Subsystem modules SHALL operate on that central state and SHALL NOT duplicate ownership of compositor-global resources across separate subsystem owners.

#### Scenario: Ownership remains centralized after extraction
- **GIVEN** subsystem code is split across multiple files
- **WHEN** ownership of compositor-global resources is inspected
- **THEN** wlroots handles, listener containers, input queues, focus metadata, cursor metadata, pointer-constraint state, and presented-frame state still resolve to one compositor state authority
- **AND** teardown ordering remains auditable from that central state

### Requirement: Responsibility-Oriented Compositor Modules
The compositor implementation SHALL organize subsystem logic into responsibility-oriented modules so future edits can stay local to one compositor concern.

The split SHALL provide module boundaries that match or closely approximate these responsibilities:

- facade/public API in `compositor_server.*`
- bootstrap/thread lifecycle/teardown in `compositor_core.*`
- input queue injection and dispatch in `compositor_input.*`
- focus switching, hit-testing, and pointer-constraint ownership in `compositor_focus.*`
- cursor setup/update/rendering in `compositor_cursor.*`
- presented-frame/render/export logic in `compositor_present.*`
- XDG lifecycle in `compositor_xdg.*`
- XWayland lifecycle in `compositor_xwayland.*`
- layer-shell lifecycle/render integration in `compositor_layer_shell.*`

The implementation SHALL NOT introduce a generic `misc`, `helpers`, or `utils` dumping-ground module for compositor extraction.
`compositor_core.*` SHALL contain only startup, shutdown, backend/output/event-loop orchestration, compositor thread lifecycle, and teardown responsibilities.
Layer-shell-originated `xdg_popup` hook creation and destruction SHALL remain owned by `compositor_xdg.*`, with `compositor_layer_shell.*` limited to forwarding popup creation events into that XDG-owned path.

#### Scenario: Localized edit surface for protocol lifecycle
- **GIVEN** a future change only affects XWayland lifecycle behavior
- **WHEN** an implementer identifies the primary edit surface
- **THEN** the primary implementation surface is `compositor_xwayland.*`
- **AND** unrelated input, cursor, and presentation logic does not need to remain in the same translation unit

#### Scenario: Localized edit surface for input targeting
- **GIVEN** a future change only affects hit-testing, focus targeting, or pointer-constraint behavior
- **WHEN** an implementer identifies the primary edit surface
- **THEN** the primary implementation surface is `compositor_focus.*`
- **AND** protocol lifecycle code does not need to be loaded to make that focused change

### Requirement: Extraction Contract Is Explicit for Apply
The change artifacts SHALL define a compile-safe extraction order and verification plan that minimize behavior drift during apply.

The change artifacts SHALL specify:

- An initial declaration-seam step that creates only the narrow internal headers needed to make multi-file extraction compile-safe.
- Updating `src/compositor/CMakeLists.txt` as each compositor translation unit lands so the migration remains compile-complete.
- Extract XDG lifecycle before XWayland lifecycle.
- Extract XWayland lifecycle before splitting input dispatch from focus/hit-testing.
- Extract layer-shell after XDG popup ownership is isolated and after the input/focus split is stable, and before final core/facade cleanup.
- Extract cursor and presentation/export logic after protocol and input/focus seams are stable.
- Leave bootstrap/teardown consolidation for the end unless an earlier minimal core split is required for safe extraction.
- Include verification commands and a concrete checklist covering startup/shutdown, Wayland clients, XWayland clients, input forwarding, focus targeting, pointer constraints, layer-shell behavior, cursor behavior, and presented-frame acquisition/export.

#### Scenario: Migration order is explicit for implementation
- **GIVEN** implementation starts from the repository artifacts alone
- **WHEN** the artifacts are read before editing code
- **THEN** the OpenSpec artifacts specify the required extraction order
- **AND** the implementation does not depend on undocumented external context to determine safe sequencing

#### Scenario: Behavior preservation is verified explicitly
- **GIVEN** the refactor is implemented
- **WHEN** verification evidence is recorded
- **THEN** the recorded verification evidence includes preset-driven build/test/static checks
- **AND** it includes a compositor behavior checklist covering startup/shutdown, Wayland/XWayland handling, input, focus, pointer constraints, layer-shell behavior, cursor, and presentation/export preservation

#### Scenario: Input-routing fallback is explicit and bounded
- **GIVEN** automated execution of `goggles_auto_input_forwarding_x11` or `goggles_auto_input_forwarding_wayland` is unavailable
- **WHEN** equivalent interactive runtime conditions exist
- **THEN** the verification plan allows manual fallback only for those unavailable input-routing checks
- **AND** it requires recorded prerequisites, observations, and stored proof for the fallback run

#### Scenario: Presentation and export verification stays mandatory
- **GIVEN** implementation touches presented-frame or DMA-BUF export code
- **WHEN** verification is executed
- **THEN** `goggles_headless_integration*` remains a required check
- **AND** the verification plan does not replace that check with manual fallback

### Requirement: Behavior Is Preserved Across the Module Split
The compositor refactor SHALL preserve existing compositor behavior while changing only the internal module layout.

The preserved behavior SHALL include:

- startup and shutdown ordering
- Wayland XDG toplevel and popup lifecycle behavior
- XWayland lifecycle behavior and X11-specific quirks
- input forwarding and focus targeting behavior
- pointer-constraint activation, confinement, and cursor-hint behavior
- layer-shell render ordering, popup forwarding, and exclusive keyboard-focus behavior
- cursor visibility, positioning, and overlay rendering behavior
- presented-frame acquisition, retained-frame refresh, and DMA-BUF export behavior

#### Scenario: Protocol and input behavior remain unchanged
- **GIVEN** the compositor implementation has been split into subsystem-oriented files
- **WHEN** Wayland clients, XWayland clients, and forwarded input are exercised through the defined verification plan
- **THEN** their externally observable behavior matches the pre-refactor compositor behavior

#### Scenario: Presentation and export behavior remain unchanged
- **GIVEN** the compositor implementation has been split into subsystem-oriented files
- **WHEN** the presented-frame path and export path are exercised through the defined verification plan
- **THEN** retained-frame behavior, cursor overlay behavior, and DMA-BUF export behavior remain unchanged

### Requirement: Behavior-Critical Quirks Stay With Their Subsystems
The compositor refactor SHALL preserve existing behavior-critical quirks, workarounds, and constraint comments adjacent to the subsystem logic they govern.

This includes XWayland-specific activation/input quirks, destroy-listener restrictions, pointer-constraint confinement/cursor-hint rules, layer-shell popup forwarding ownership, and hook-allocation/lifetime constraints for protocol handlers.

#### Scenario: XWayland quirks remain isolated and documented
- **GIVEN** XWayland handling is extracted
- **WHEN** the subsystem ownership is inspected
- **THEN** XWayland-specific activation and destroy-listener constraints remain documented next to the XWayland lifecycle logic
- **AND** those constraints are not moved into an unrelated shared helper or omitted during extraction
