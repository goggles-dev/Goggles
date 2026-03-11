# SDD Interview Spec: Filter Chain Diagnostics System

## Metadata

- Interview ID: fcd-2026-0310-001
- Change Name: filter-chain-diagnostics
- Rounds: 4
- Final Ambiguity Score: 19.5%
- Type: brownfield
- Generated: 2026-03-10
- Threshold: 0.2
- Status: PASSED

## Clarity Breakdown

| Dimension          | Score | Weight | Weighted      |
| ------------------ | ----- | ------ | ------------- |
| Goal Clarity       | 0.90  | 0.35   | 0.315         |
| Constraint Clarity | 0.70  | 0.25   | 0.175         |
| Success Criteria   | 0.75  | 0.25   | 0.188         |
| Context Clarity    | 0.85  | 0.15   | 0.128         |
| **Total Clarity**  |       |        | **0.805**     |
| **Ambiguity**      |       |        | **0.195**     |

## Goal

Design a comprehensive diagnostics and debugging system for the multi-pass filter chain architecture. The system must validate three classes of failures: (1) syntax/authoring errors in shader source and presets, (2) runtime/binding/execution errors during GPU pipeline operation, (3) output-quality/semantic-correctness errors in rendered results. The design document must be complete and evidence-driven, covering all three failure classes comprehensively. Implementation is expected to be phased.

## Constraints

- **Activation model:** Layered — compile-time gate (like Tracy) for heavy instrumentation with zero overhead when disabled, plus lightweight always-on validation for cheap checks that runs every frame.
- **Integration model:** Adapter pattern — sink-agnostic core diagnostic engine that emits to abstract sinks, with adapters routing to existing infrastructure (spdlog, Tracy, ImGui, headless capture) and future backends.
- **Scope:** Full design covering all three failure classes. Implementation can be phased, but the design must be complete.
- **Output format:** Markdown report written to project root.
- **Abstraction level:** The report must describe everything in terms of mechanisms, responsibilities, data flow, execution stages, and observability boundaries — not specific code symbols.

## Non-Goals

- Direct implementation of the diagnostics system (this is a design/exploration task)
- Modifying existing code paths during this change
- Building a standalone shader IDE or external tool

## Acceptance Criteria

- [ ] Report reconstructs the engine's current model from code evidence (pipeline structure, shader flow, parameter resolution, semantic injection, resource routing, execution model, output composition)
- [ ] Design validates syntax/authoring errors: observable state, instrumentation points, invariants, structured artifacts, failure localization, developer evidence
- [ ] Design validates runtime/binding/execution errors: same coverage as above
- [ ] Design validates output-quality/semantic-correctness errors: same coverage as above
- [ ] Design includes layered diagnostics architecture with compile-time and runtime tiers
- [ ] Design includes adapter-pattern sink-agnostic core with concrete adapter specifications
- [ ] Design includes full reporting/capture modes
- [ ] Design includes static validation, runtime validation, and quality-validation workflows
- [ ] Design includes intermediate-output inspection strategy
- [ ] Design includes parameter/semantic conformance strategy
- [ ] Design includes automated regression and golden-output strategy
- [ ] Design includes operational workflow for investigating failures
- [ ] Design addresses all four key scenarios: incorrect rendering, pipeline regression, compilation failure, performance bottleneck
- [ ] Design includes scalability, performance, and developer-experience tradeoffs
- [ ] Design includes risks, blind spots, and mitigation strategies

## Assumptions Exposed & Resolved

| Assumption | Challenge | Resolution |
| --- | --- | --- |
| Diagnostics should be always-on | Would always-on overhead be acceptable? | Layered: compile-time gate for heavy, always-on for cheap |
| Should extend existing infra | Build on existing vs. self-contained? | Adapter pattern: sink-agnostic core with adapters |
| Runtime diagnostics alone sufficient | Would just runtime errors be valuable enough? | Full design required, all three classes, phased implementation |
| Single primary scenario | Which scenario defines the acceptance bar? | All four scenarios are equally critical |

## Technical Context

### Existing Diagnostic Infrastructure (Mature)
- **Error handling:** Result<T>/ResultPtr<T> with ErrorCode enum, GOGGLES_TRY/GOGGLES_MUST macros, source location capture
- **Logging:** spdlog with configurable levels, console+file sinks, [VVL] prefix for Vulkan validation
- **Vulkan validation:** VK_LAYER_KHRONOS_validation integrated via debug messenger, severity-based routing
- **Profiling:** Tracy macros (GOGGLES_PROFILE_FUNCTION/SCOPE/TAG/VALUE), zero-overhead when disabled
- **Sanitizers:** ASAN/UBSAN in CMake presets
- **Runtime metrics:** FPS + latency with 120-frame rolling history, ImGui visualization
- **ImGui overlay:** Shader controls, performance dashboard, real-time parameter tuning
- **Frame capture:** Headless mode with PNG export via stb_image_write

### Existing Diagnostic Gaps
- Shader compilation error details not extracted/formatted for display
- No intermediate texture inspection (per-pass output viewing)
- No per-pass GPU timing breakdown
- No structured diagnostic artifact emission
- No SPIR-V dump/disassembly on demand
- No GPU memory tracking or pressure warnings
- No automated regression/golden-output framework
- No command buffer debug labels

### Pipeline Architecture
- **Stages:** Pre-chain (downsample) → Main chain (N filter passes) → Post-chain (output blit)
- **Shader flow:** .slangp preset parsing → .slang preprocessing (stage split, parameter extraction) → Slang compilation → SPIR-V + reflection
- **Semantic injection:** Per-frame UBO + push constant updates (source/output/original sizes, frame count, viewport, MVP)
- **Resource routing:** Framebuffers per pass, feedback buffers, frame history (circular 7-deep), texture registry, alias-based texture references
- **Execution:** Single command buffer recording with layout transitions between passes

## Ontology (Key Entities)

| Entity | Fields | Relationships |
| --- | --- | --- |
| DiagnosticEvent | severity, category, pass_index, stage, message, evidence | emitted_by Pass/Pipeline, routed_to Sink |
| DiagnosticSink | type, enabled, filter_policy | receives DiagnosticEvent |
| PassDiagnostics | pass_index, input_extent, output_extent, timing, texture_bindings | belongs_to FilterPass |
| ValidationRule | target_class, check_fn, severity, description | validates Pass/Pipeline/Shader |
| CaptureFrame | frame_index, pass_outputs[], parameter_state, semantic_state | captured_during Execution |
| RegressionBaseline | preset_name, golden_outputs[], tolerance | compared_against CaptureFrame |

## Interview Transcript

<details>
<summary>Full Q&A (4 rounds)</summary>

### Round 1

**Q:** Should the diagnostics system be always-on (runtime overhead every frame), opt-in at runtime (config/CLI toggle), opt-in at compile-time (like Tracy's zero-overhead pattern), or a standalone offline tool?
**A:** Layered: compile-time + runtime — Zero-overhead compile gate for heavy instrumentation, plus lightweight always-on validation that costs near-zero.
**Ambiguity:** 59% → 41% (Goal: 0.70, Constraints: 0.50, Criteria: 0.40, Context: 0.80)

### Round 2

**Q:** What is the primary scenario where this diagnostics system must prove its value?
**A:** All of them — new shader renders incorrectly, regression after pipeline change, shader compilation/load failure, and performance bottleneck isolation are all equally critical.
**Ambiguity:** 41% → 34% (Goal: 0.75, Constraints: 0.50, Criteria: 0.60, Context: 0.85)

### Round 3

**Q:** Should this system compose the existing diagnostic infrastructure (spdlog, Tracy, ImGui, headless capture) or be a self-contained parallel system?
**A:** Adapter pattern (sink-agnostic) — Core diagnostic engine is independent and can emit to any sink, with adapters that route to spdlog, Tracy, ImGui.
**Ambiguity:** 34% → 28% (Goal: 0.80, Constraints: 0.65, Criteria: 0.60, Context: 0.85)

### Round 4 (Contrarian)

**Q:** What if you only built runtime/binding diagnostics and deferred static validation and golden-output regression? Would the system still justify its complexity?
**A:** Full design, phased implementation — The design document must cover all three failure classes comprehensively. Implementation can be phased, but the design must be complete.
**Ambiguity:** 28% → 19.5% (Goal: 0.90, Constraints: 0.70, Criteria: 0.75, Context: 0.85)

</details>
