# Dual-Host Pacing Validation Fallback

- fallback_name: `dual-host-pacing-validation-fallback`
- local_runtime_proof: `printenv DISPLAY WAYLAND_DISPLAY XDG_SESSION_TYPE` returned `DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-0`, and `XDG_SESSION_TYPE=wayland`. This confirms a Wayland-hosted session with XWayland available, not a true pair of separate Wayland-host and X11-host runtimes for the required dual-host acceptance pass.

| Target | Host type | Observed FPS window | Proof location |
| --- | --- | --- | --- |
| Active capture target driven from the Application window pacing controls | Wayland host | Unobserved in this apply run; a manual host run is still required to record capped and uncapped FPS behavior over the required observation window. | `openspec/changes/present-workflow-add-frame-pacing/manual-host-validation-fallback.md` |
| Active capture target driven from the Application window pacing controls | X11 host | Unobserved in this apply run; `DISPLAY=:0` came from XWayland inside a Wayland-hosted session, so a separate X11-host session is still required for true X11-host acceptance. | `openspec/changes/present-workflow-add-frame-pacing/manual-host-validation-fallback.md` |
