# Proposal: Filter Chain Diagnostics and Debugging System

## Intent

The filter chain's multi-pass pipeline is powerful but opaque when things go wrong. Today, a shader that compiles but renders incorrectly gives developers almost no structured evidence to localize the problem to a specific pass, binding, semantic, or resource. Silent fallback substitution (replacing missing textures with the source image) masks binding bugs. Reflection loss after successful compilation degrades silently into confusing runtime behavior. Intermediate pass outputs are invisible — only the final composited output can be inspected.

This change designs a comprehensive diagnostics system that makes silent degradation impossible, localizes failures to a specific pass/stage/resource/semantic, and produces structured evidence for both developer triage and automated regression. The design covers three failure classes: syntax/authoring errors, runtime/binding/execution errors, and output-quality/semantic-correctness errors.

**Why now:** The filter chain boundary has stabilized (standalone `goggles-filter-chain` target, C API, boundary-safe contracts). The existing observability primitives (spdlog, Tracy, Vulkan validation, headless capture, image comparison) provide building blocks but are not connected into a cohesive diagnostic system.

## Scope

### In Scope

- **Layered diagnostics architecture** with three tiers: always-on lightweight validation (Tier 0), runtime opt-in capture and tracing (Tier 1, config-toggled), compile-time gated forensic instrumentation (Tier 2, zero-overhead when disabled)
- **Sink-agnostic diagnostic core** using adapter pattern: core emits structured events to abstract sinks; adapters route to spdlog, Tracy, ImGui, headless capture, and a test-harness collector
- **Authoring analysis layer** (static validation): preset normalization, include/redirect graph building, source provenance mapping, compile report with source-mapped diagnostics, reflection conformance gate, parameter/semantic conformance validation
- **Runtime validation layer**: binding ledger (per-pass resolved resource table with fallback status), semantic assignment ledger, degradation ledger, execution timeline with per-pass timing, generation-aware rebuild/swap validation
- **Capture and inspection layer**: intermediate pass output readback (on-demand and on-failure), history/feedback surface capture, semantic/parameter snapshot, metadata sidecars, reproducibility envelope
- **Quality validation layer**: golden comparison harness for per-pass intermediate outputs (not just final output), semantic-probe presets for contract testing, temporal-consistency validation, diff heatmap generation
- **Four reporting modes**: Minimal (verdict + error counts), Standard (manifest + coverage tables + one-frame trace), Investigate (intermediate captures + detailed ledgers), Forensic (full temporal capture + complete artifact bundle)
- **Operational workflows** for four key scenarios: incorrect rendering, pipeline regression, compilation failure, performance bottleneck isolation
- **Phased rollout plan** with four implementation phases

### Out of Scope

- Direct implementation of the diagnostics system (this change produces the design document and proposal only)
- Modifying existing rendering code paths
- Building a standalone external tool or shader IDE
- Diagnostics for the compositor/capture pipeline (Wayland surface management, DMA-BUF import/export)
- GPU memory pressure monitoring (deferred to a future change)

## Approach

The design report (`filter-chain-diagnostics-design.md`) reconstructs the engine model from code evidence, then specifies a five-layer diagnostics architecture:

1. **Layer A: Policy & Session Control** — defines capture depth, retention policy, strict vs. compatibility mode, session identity with content hashes
2. **Layer B: Authoring Analysis** — static validation before any GPU execution; produces chain manifest, source provenance, compile/reflection reports, authoring verdict
3. **Layer C: Runtime Validation** — validates bindings, semantics, temporal resources, and fallback substitutions during live frame recording; produces binding/semantic/degradation ledgers
4. **Layer D: Capture & Inspection** — optional deep evidence: intermediate images, semantic snapshots, push constant dumps, reproducibility envelopes
5. **Layer E: Quality Validation** — golden comparisons, semantic-probe presets, temporal-consistency checks, diff heatmaps, localization from output deltas to upstream passes

The core is sink-agnostic: a structured event model with severity, category, pass localization, and evidence payload. Adapters route events to existing infrastructure (spdlog for logging, Tracy for timing, ImGui for live inspection, headless capture for regression). A test-harness sink enables automated assertion on diagnostic events.

The activation model is layered: Tier 0 always-on checks cost < 1% frame time. Tier 1 runtime opt-in (GPU timestamp queries, readback) costs 5–15%. Tier 2 compile-time gated forensic capture follows Tracy's zero-overhead pattern.

Implementation is designed for four phases: (1) contract visibility, (2) runtime truth, (3) image/temporal evidence, (4) regression hardening.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/render/chain/` | Modified | Instrumentation points for binding ledger, semantic ledger, pass-level events, intermediate capture hooks |
| `src/render/shader/` | Modified | Compile report emission, source provenance mapping, reflection conformance gate |
| `src/render/backend/` | Modified | GPU timestamp query infrastructure, debug label insertion, readback staging |
| `src/render/texture/` | Modified | Texture lifecycle tracking, capture readback support |
| `src/util/` | New + Modified | Diagnostic event model, sink interface, adapter registry, diagnostic configuration |
| `src/ui/` | Modified | Diagnostic inspection panels (intermediate texture gallery, binding table, semantic table) |
| `tests/render/` | New + Modified | Diagnostic event assertion tests, authoring validation tests, conformance test harness |
| `tests/visual/` | Modified | Extended golden baseline infrastructure for per-pass intermediate outputs |
| `config/goggles.template.toml` | Modified | New `[diagnostics]` section in the shipped template; first-run bootstrap materializes it into `${XDG_CONFIG_HOME:-$HOME/.config}/goggles/goggles.toml` when the runtime user config is missing |

## Impacted OpenSpec Specs

| Spec | Impact |
|------|--------|
| `render-pipeline` | Modified — new requirements for diagnostic instrumentation points, compile reports, reflection conformance |
| `goggles-filter-chain` | Modified — boundary API extensions for diagnostic event emission and capture control |
| `shader-testing` | Modified — extended to include structured compile reports, authoring validation corpus |
| `visual-regression` | Modified — extended to support per-pass intermediate golden baselines, not just final output |
| `profiling` | Modified — per-pass GPU timestamp queries complement Tracy CPU profiling |
| `diagnostics` (new) | New spec domain covering the diagnostic event model, sink contracts, and validation workflows |

## Policy-Sensitive Impacts

- **Error handling:** Diagnostic events are NOT errors in the `Result<T>` sense. They are structured observations. The existing error propagation path remains unchanged. Diagnostics operate alongside, not instead of, error handling.
- **Logging:** Diagnostic log sink routes through spdlog but uses a dedicated logger name to avoid flooding the main application log. Diagnostic events carry structured metadata beyond what spdlog formats natively.
- **Vulkan API:** GPU timestamp queries and debug labels use `vk::` APIs exclusively. Readback staging uses existing `vk::Buffer` allocation patterns.
- **Threading:** Diagnostic event emission in the render path is single-threaded (same as render recording). Async serialization to disk uses the existing job system.
- **Lifetime/ownership:** Diagnostic sinks are owned by the diagnostic session, which is scoped to the application lifetime. Capture buffers follow the same RAII patterns as existing framebuffers.

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Adapter pattern adds complexity without enough sink diversity to justify it | Medium | Start with 2 sinks (log + test-harness). Adapter interface is narrow (one method). Add UI/capture sinks in later phases. |
| GPU readback stalls for intermediate capture degrade frame timing | Medium | Use async readback with fence-based staging. Tier 1 captures are opt-in and can be scoped to specific passes/frames. |
| Diagnostic overhead masks real performance issues during profiling | Low | Tier 0 checks are < 1% overhead. Tier 1 timing measurements exclude diagnostic operations from their own measurements. |
| Golden baselines for intermediate outputs are GPU-vendor dependent | High | Use scenario-specific tolerances with per-device profiles. Keep a tightly controlled deterministic smoke subset. Use perceptual metrics alongside exact diffs. |
| Forensic capture disk volume becomes unmanageable | Medium | Tiered capture modes with configurable retention. Hash unchanged outputs to skip re-capture. Hard disk space limit with rotation. |
| Silent fallback removal in strict mode breaks existing preset loading | Low | Strict mode is opt-in (CI default). Compatibility mode preserves current behavior but records degradation events. Migration path documented. |

## Rollback Plan

This change produces a design document only — no code is modified. Rollback is trivial:

- Remove `filter-chain-diagnostics-design.md` from project root
- Remove `openspec/changes/filter-chain-diagnostics/` directory
- No code changes to revert

For future implementation phases, each phase is independently revertible:
- Phase 1 (contract visibility): revert new diagnostic source files and test additions
- Phase 2 (runtime truth): revert instrumentation points in chain/shader modules
- Phase 3 (capture): revert readback infrastructure and capture hooks
- Phase 4 (regression): revert golden baseline extensions and new test presets

## Dependencies

- Existing `goggles-filter-chain` boundary API (stable, already shipped)
- Existing spdlog, Tracy, ImGui infrastructure (mature, no changes needed to core libraries)
- Existing headless mode and image comparison library (stable foundation for quality validation)
- Existing Catch2 test infrastructure (harness for diagnostic event assertions)

## Success Criteria

- [ ] Design report reconstructs the engine's current model from code evidence covering all 7 subsystems (pipeline structure, shader flow, parameter resolution, semantic injection, resource routing, execution model, output composition)
- [ ] Design validates all three failure classes with observable state, instrumentation points, invariants, structured artifacts, failure localization, and evidence presentation strategies
- [ ] Design specifies a layered architecture with Tier 0 (always-on), Tier 1 (runtime opt-in), and Tier 2 (compile-time gated) activation
- [ ] Design specifies sink-agnostic core with concrete adapter specifications for at least spdlog, Tracy, ImGui, and test-harness sinks
- [ ] Design includes four reporting modes (Minimal, Standard, Investigate, Forensic) with clear scoping rules
- [ ] Design addresses all four key scenarios: incorrect rendering localization, pipeline regression detection, compilation failure diagnosis, performance bottleneck isolation
- [ ] Design includes phased rollout plan with independently implementable and revertible phases
- [ ] Design identifies risks and blind spots with concrete mitigation strategies
- [ ] Design report is written to project root as `filter-chain-diagnostics-design.md` using only mechanism/data-flow language (no code symbols)
