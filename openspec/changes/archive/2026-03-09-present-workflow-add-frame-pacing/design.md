## Context

Goggles already has a viewer-side pacing path: `render.target_fps` flows through config/CLI into the
render backend, which uses `VK_KHR_present_wait` when available and a CPU sleep fallback when it is
not. That contract does not currently bound the nested compositor's callback cadence, so target apps
inside the wlroots/XWayland compositor can still run far faster than the selected target even while
the viewer presents at the desired rate.

This change crosses `src/compositor`, `src/render/backend`, `src/app`, `src/ui`, and `src/util`.
The design MUST preserve one pacing target, keep viewer pacing reuse explicit, and avoid inventing a
second independent policy surface that can drift across the workflow.

## Goals / Non-Goals

**Goals:**
- Make one global target FPS govern the full `app -> compositor -> viewer` present workflow.
- Keep compositor pacing mandatory so client callback cadence no longer ignores the selected target.
- Reuse the existing viewer present-wait and CPU-throttle subsystem instead of duplicating viewer
  pacing logic.
- Surface runtime pacing controls in the Application window without breaking existing config/CLI
  target-FPS semantics.
- Preserve uncapped mode as `target_fps = 0`.
- Keep v1 verification minimal and observable: +/-3 FPS of target over 10 seconds on Wayland and X11
  hosts.

**Non-Goals:**
- Separate app, compositor, and viewer FPS targets in v1.
- A viewer-only fix that leaves compositor callback pacing uncapped.
- Persisting runtime ImGui changes back into the config file.
- A broader render/compositor architecture rewrite.
- A second v1 acceptance metric for jitter beyond the FPS tolerance contract.

## Decisions

### Decision: Keep one boundary-owned effective pacing target

The application SHALL resolve one effective pacing target for the current session and SHALL push that
value through one runtime update path into both the public `CompositorServer` pacing seam and viewer
pacing state.

Rationale:
- The proposal and delta specs define one global target as the v1 pacing contract.
- A single boundary-owned value routed through the application/compositor seam prevents the
  compositor and viewer from drifting after runtime UI
  changes.

Alternatives considered:
- Independent stage-specific targets: rejected because it expands scope and creates ambiguous user
  behavior.
- Viewer-owned target with compositor heuristics: rejected because the compositor must be a
  first-class pacing participant.

### Decision: Reuse the existing viewer pacing subsystem as the viewer half of the contract

The render backend SHALL continue to use the existing `RenderOutput` present-wait and CPU-throttle
logic for the viewer side of the pacing contract. The new work SHALL feed the shared target into that
existing mechanism rather than replace it.

Rationale:
- The current viewer pacing path already encodes the supported Vulkan extension and fallback rules.
- Reuse reduces maintenance burden and keeps the authoritative render-pipeline policy in one place.

Alternatives considered:
- Rewrite viewer pacing together with compositor pacing: rejected because it expands risk without
  solving a new requirement.
- Add a second wrapper-specific pacing layer above `RenderOutput`: rejected because it duplicates the
  existing policy surface.

### Decision: Pace compositor callback publication with compositor-owned scheduler state

The compositor SHALL stop relying on immediate `frame_done` issuance as the sole pacing mechanism for
the active capture target. Instead, it SHALL use compositor-owned pacing state to decide when the next
callback/publication is eligible while preserving uncapped behavior when `target_fps = 0`.

Rationale:
- Immediate callback issuance is the root of the current runaway nested-compositor behavior.
- The compositor event loop is the correct ownership boundary for capture callback cadence.

Alternatives considered:
- Pace only the exported DMA-BUF publication while leaving callback cadence uncapped: rejected because
  the target app can still free-run.
- Pace every surface uniformly: rejected because the current requirements focus on the active capture
  target and bounded first-version scope.

### Decision: Make runtime pacing controls session-scoped UI state that updates both halves together

The Application window SHALL expose runtime pacing controls that initialize from the resolved
config/CLI target and update the active session target without rewriting config on disk. A runtime UI
change SHALL update compositor and viewer pacing together through one application-owned callback path.

Rationale:
- The request explicitly requires ImGui configuration support.
- Session-scoped runtime control avoids introducing config-write policy in this change while still
  making pacing debuggable live.

Alternatives considered:
- Config/CLI only: rejected because it leaves no runtime control surface.
- Persist UI edits back to TOML immediately: rejected because it adds unrelated config-write behavior.

### Decision: Keep host-aware acceptance explicit but backend timing-source-agnostic

The contract SHALL explicitly require acceptance on both Wayland and X11 hosts, but the pacing policy
shall remain grounded in Goggles-owned timing state rather than depending on a host-compositor-
specific feedback API.

Rationale:
- The proposal and delta specs require the same acceptance contract on both Wayland and X11 hosts.
- Goggles can control its own pacing state more deterministically than host-specific signal surfaces.

Alternatives considered:
- Wayland-only first version: rejected because the proposal and delta specs require one acceptance
  contract across both Wayland and X11 hosts.
- Separate host-specific pacing contracts: rejected because one v1 contract is simpler and more
  testable.

## Risks / Trade-offs

- [Compositor responsiveness drift] -> bound compositor pacing to the active capture path only and
  keep uncapped mode explicit.
- [Target-value drift between UI, compositor, and viewer] -> use one application-owned runtime update
  path and keep one effective target value.
- [Host-specific acceptance flakiness] -> make Wayland and X11 validation explicit in tasks and keep
  the numeric rule simple for v1.
- [Scope creep into persistence or multi-target policy] -> keep runtime UI changes session-scoped and
  defer per-stage targets.

## Migration Plan

1. Define the OpenSpec contract changes for render pacing, compositor participation, and Application
   window runtime controls.
2. Add shared runtime pacing state and update plumbing from config/CLI/UI through the Application
   boundary.
3. Introduce compositor-owned callback pacing for the active capture target while preserving uncapped
   mode.
4. Reuse existing viewer pacing logic for the viewer half of the shared target.
5. Verify the shared target and runtime controls on both Wayland and X11 hosts.

Rollback strategy:
- Revert the change as one unit if shared pacing state or compositor callback pacing proves unstable;
  do not keep split viewer/compositor behavior hidden behind the same target-FPS contract.

## Open Questions

- None. Remaining implementation details are bounded to code-path mapping and verification, not
  product-definition ambiguity.
