# Tasks: Filter Chain Diagnostics and Debugging System

## Phase 1: Foundation — Core Diagnostic Types, Event Model, Session, and Sinks

### 1.0 Build Infrastructure

- [x] 1.0.1 Create `src/util/diagnostics/CMakeLists.txt` defining a `goggles_diagnostics` OBJECT library. Add source files incrementally as they are created. Link against `goggles_util` and `Vulkan::Vulkan`. Apply `goggles_enable_clang_tidy`, `goggles_enable_sanitizers`, and `goggles_enable_profiling`.
  - **Verify:** `pixi run build -p debug` succeeds with the empty library target.

- [x] 1.0.2 Wire `goggles_diagnostics` into the build graph by adding `add_subdirectory(diagnostics)` to `src/util/CMakeLists.txt`, and linking `goggles_diagnostics` into `goggles_render_chain_obj` in `src/render/chain/CMakeLists.txt`.
  - **Verify:** `pixi run build -p debug` succeeds; `goggles_diagnostics` objects appear in the link.

### 1.1 Diagnostic Event Model

- [x] 1.1.1 Create `src/util/diagnostics/diagnostic_event.hpp` defining `goggles::diagnostics::Severity` (debug, info, warning, error), `Category` (authoring, runtime, quality, capture), `LocalizationKey` (pass_ordinal with `CHAIN_LEVEL` sentinel, stage `string_view`, resource `string_view`), evidence payload variants (`BindingEvidence`, `SemanticEvidence`, `CompileEvidence`, `ReflectionEvidence`, `ProvenanceEvidence`, `CaptureEvidence`), `EvidencePayload` as `std::variant<std::monostate, ...>`, and `DiagnosticEvent` struct per the design interfaces section.
  - **Verify:** `pixi run build -p debug` compiles. Header is include-only (no .cpp needed).

- [x] 1.1.2 Create `tests/render/test_diagnostic_event_model.cpp` with Catch2 tests: severity ordering (debug < info < warning < error), `LocalizationKey::CHAIN_LEVEL` sentinel value correctness, `DiagnosticEvent` construction with each evidence variant, `std::monostate` default evidence. Add to `tests/CMakeLists.txt` under `goggles_tests` sources.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes diagnostic event model tests.

### 1.2 Diagnostic Policy

- [x] 1.2.1 Create `src/util/diagnostics/diagnostic_policy.hpp` defining `PolicyMode` (compatibility, strict), `CaptureMode` (minimal, standard, investigate, forensic), `ActivationTier` (tier0, tier1, tier2), and `DiagnosticPolicy` struct with defaults per design. Include severity promotion logic: `promote_fallback_to_error` derived from `mode == strict`, `reflection_loss_is_fatal` derived from `mode == strict`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 1.2.2 Add policy unit tests to `tests/render/test_diagnostic_event_model.cpp` (or a new `tests/render/test_diagnostic_policy.cpp`): strict mode sets `promote_fallback_to_error = true`; compatibility mode keeps it `false`; tier values are ordered; default policy is compatibility/standard/tier0. Add new file to `tests/CMakeLists.txt` if separate.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes policy tests.

### 1.3 Sink Interface and Concrete Sinks

- [x] 1.3.1 Create `src/util/diagnostics/diagnostic_sink.hpp` defining abstract `DiagnosticSink` with single pure virtual `void receive(const DiagnosticEvent& event)` and virtual destructor.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 1.3.2 Create `src/util/diagnostics/log_sink.hpp` and `src/util/diagnostics/log_sink.cpp` implementing `LogSink` that formats `DiagnosticEvent` fields and routes to spdlog via a dedicated logger name `"diagnostics"`. Map `Severity` to spdlog levels. Add `log_sink.cpp` to `src/util/diagnostics/CMakeLists.txt`.
  - **Verify:** `pixi run build -p debug` compiles with `LogSink`.

- [x] 1.3.3 Create `src/util/diagnostics/test_harness_sink.hpp` and `src/util/diagnostics/test_harness_sink.cpp` implementing `TestHarnessSink` that collects events in a `std::vector<DiagnosticEvent>`, supports `events_by_category(Category)`, `events_by_severity(Severity)`, `event_count()`, and `clear()`. Add `test_harness_sink.cpp` to `src/util/diagnostics/CMakeLists.txt`.
  - **Verify:** `pixi run build -p debug` compiles with `TestHarnessSink`.

- [x] 1.3.4 Create `tests/render/test_diagnostic_sinks.cpp` with Catch2 tests: `TestHarnessSink` collects events in emission order; filter by category returns only matching events; filter by severity returns only matching events; `LogSink` does not throw on receive (verify via test-harness that spdlog logger is invoked). Add to `tests/CMakeLists.txt`.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes sink tests.

### 1.4 Session Identity

- [x] 1.4.1 Create `src/util/diagnostics/session_identity.hpp` defining `SessionIdentity` struct: `preset_hash`, `expanded_source_hash`, `compiled_contract_hash` (all `std::string`), `generation_id` (`uint64_t`), `frame_start`/`frame_end` (`uint32_t`), `capture_mode` (`std::string`), `environment_fingerprint` (`std::string`).
  - **Verify:** `pixi run build -p debug` compiles.

### 1.5 Ledgers

- [x] 1.5.1 Create `src/util/diagnostics/degradation_ledger.hpp` defining `DegradationLedger` with `DegradationEntry` (pass ordinal, expected resource, substituted resource, frame index, degradation type enum). Provide `record(...)`, `entries_for_pass(uint32_t)`, `all_entries()`, `clear()`. Header-only or minimal .cpp.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 1.5.2 Create `src/util/diagnostics/binding_ledger.hpp` defining `BindingLedger` with `BindingEntry` (pass ordinal, binding slot, status enum: resolved/substituted/unresolved, resource identity, extent w/h/format, producer pass ordinal, alias name). Provide `record(...)`, `entries_for_pass(uint32_t)`, `all_entries()`, `clear()`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 1.5.3 Create `src/util/diagnostics/semantic_ledger.hpp` defining `SemanticAssignmentLedger` with `SemanticEntry` (pass ordinal, member name, classification enum: parameter/semantic/static/unresolved, value as `std::variant<float, std::array<float,4>>`, offset). Provide `record(...)`, `entries_for_pass(uint32_t)`, `all_entries()`, `clear()`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 1.5.4 Create `src/util/diagnostics/execution_timeline.hpp` defining `ExecutionTimeline` with `TimelineEvent` (event type enum: pass_start/pass_end/history_push/feedback_copy/allocation, pass ordinal, cpu_timestamp_ns, gpu_duration_us optional). Provide `record(...)`, `events()`, `events_for_pass(uint32_t)`, `clear()`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 1.5.5 Create `tests/render/test_binding_ledger.cpp` with Catch2 tests: record entries across multiple passes; query by pass ordinal returns only that pass; status classification is correct; producer chain is recorded; extents are recorded. Add to `tests/CMakeLists.txt`.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes ledger tests.

### 1.6 Chain Manifest

- [x] 1.6.1 Create `src/util/diagnostics/chain_manifest.hpp` defining `ChainManifest` with per-pass entries (ordinal, shader path, scale type, scale factor, format override, wrap mode), alias list, texture declarations, temporal requirements (history depth, feedback producers/consumers). Provide factory `static auto from_preset(const PresetConfig&) -> ChainManifest`.
  - **Verify:** `pixi run build -p debug` compiles.

### 1.7 Diagnostic Session

- [x] 1.7.1 Create `src/util/diagnostics/diagnostic_session.hpp` defining `DiagnosticSession` class per the design interface: `create(DiagnosticPolicy)`, `emit(DiagnosticEvent)`, `register_sink(unique_ptr<DiagnosticSink>)` returning `SinkId`, `unregister_sink(SinkId)`, policy get/set, identity get/update, ledger accessors (`degradation_ledger()`, `binding_ledger()`, `semantic_ledger()`, `execution_timeline()`, `chain_manifest()`, `authoring_verdict()`), `event_count(Severity)`, `event_count(Category)`, `begin_frame(uint32_t)`, `end_frame()`, `reset()`.
  - **Verify:** `pixi run build -p debug` compiles (header only at this point).

- [x] 1.7.2 Create `src/util/diagnostics/diagnostic_session.cpp` implementing session: fan-out to registered sinks on `emit()`, populate ledgers from event category/evidence, apply policy severity promotion before fan-out, track severity/category counts, handle sink failure isolation (catch exceptions from sink `receive()` and emit a meta-event). Add to `src/util/diagnostics/CMakeLists.txt`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 1.7.3 Create `tests/render/test_diagnostic_session.cpp` with Catch2 tests: session with no sinks silently discards events; session with one sink delivers events; session with two sinks delivers to both in registration order; severity promotion in strict mode changes event severity but preserves `original_severity`; event counting by severity and category is correct; `begin_frame`/`end_frame` updates frame tracking; `reset()` clears all ledgers and counts. Add to `tests/CMakeLists.txt`.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes session tests.

### 1.8 TOML Configuration

- [x] 1.8.1 Add `Config::Diagnostics` struct to `src/util/config.hpp` with fields: `std::string mode` (default `"standard"`), `bool strict` (default `false`), `uint32_t tier` (default `0`), `uint32_t capture_frame_limit` (default `1`), `uint64_t retention_bytes` (default `256 * 1024 * 1024`). Add `Diagnostics diagnostics` member to `Config`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 1.8.2 Extend `load_config()` in `src/util/config.cpp` to parse the `[diagnostics]` TOML section. Missing keys fall back to defaults. Invalid values produce `Result<Config>` errors.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 1.8.3 Add `[diagnostics]` section to `config/goggles.template.toml` with commented-out defaults: `# mode = "standard"`, `# strict = false`, `# tier = 0`; first-run bootstrap materializes `${XDG_CONFIG_HOME:-$HOME/.config}/goggles/goggles.toml` from the template when the runtime user config is missing.
  - **Verify:** Existing config load tests still pass: `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`.

- [x] 1.8.4 Add test cases to `tests/util/test_config.cpp` for: config with `[diagnostics]` section parses correctly; config without `[diagnostics]` uses defaults; invalid tier value returns error.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes config tests.

### 1.9 Phase 1 Gate

- [x] 1.9.1 Run full CI-parity gate: `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`. All pass with zero new warnings.
  - **Verify:** All three commands succeed.


## Phase 2: Authoring Analysis — Compile Reports, Provenance, Conformance

### 2.1 Source Provenance

- [x] 2.1.1 Create `src/util/diagnostics/source_provenance.hpp` defining `SourceProvenanceMap` with `ProvenanceEntry` (original file path, original line number, rewrite flag, rewrite description). Provide `record(uint32_t expanded_line, ProvenanceEntry)`, `lookup(uint32_t expanded_line) -> const ProvenanceEntry*`, `size()`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 2.1.2 Add optional `SourceProvenanceMap*` output parameter to `RetroArchPreprocessor::preprocess()` and `preprocess_source()` in `src/render/shader/retroarch_preprocessor.hpp`. Default to `nullptr` for backward compatibility.
  - **Verify:** `pixi run build -p debug` compiles; existing preprocessor tests pass unchanged.

- [x] 2.1.3 Implement provenance tracking in `RetroArchPreprocessor::resolve_includes()` in `src/render/shader/retroarch_preprocessor.cpp`: when provenance map pointer is non-null, record expanded-line to original-file-and-line entries during include expansion. Track compatibility rewrites similarly.
  - **Verify:** `pixi run build -p debug` compiles; existing preprocessor tests pass.

- [x] 2.1.4 Add provenance-specific tests to `tests/render/test_retroarch_preprocessor.cpp` (or new `tests/render/test_source_provenance.cpp`): preprocess a shader with `#include`, verify provenance maps expanded lines to original files; preprocess a shader with compatibility rewrites, verify rewrite flag is set on affected entries. Add new file to `tests/CMakeLists.txt` if separate.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes provenance tests.

### 2.2 Compile Reports

- [x] 2.2.1 Create `src/util/diagnostics/compile_report.hpp` defining `CompileReport` with `StageReport` entries (stage enum: vertex/fragment, success bool, diagnostic messages vector with source-mapped locations, timing_us, cache_hit bool). Provide `add_stage(StageReport)`, `stages()`, `all_succeeded()`, `total_timing_us()`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 2.2.2 Add optional `CompileReport*` output parameter to `ShaderRuntime::compile_retroarch_shader()` in `src/render/shader/shader_runtime.hpp`. Default to `nullptr`.
  - **Verify:** `pixi run build -p debug` compiles; existing shader tests pass unchanged.

- [x] 2.2.3 Populate compile report in `ShaderRuntime::compile_retroarch_shader()` in `src/render/shader/shader_runtime.cpp`: record per-stage compilation success, diagnostic messages, timing (use `std::chrono::steady_clock`), and cache-hit status. Only populate when report pointer is non-null.
  - **Verify:** `pixi run build -p debug` compiles; existing shader tests pass.

- [x] 2.2.4 Add compile report tests to `tests/render/test_shader_runtime.cpp` (or new `tests/render/test_compile_report.cpp`): compile a valid shader with `CompileReport*`, verify `all_succeeded() == true` and two stage entries; compile an invalid shader, verify failure stage is recorded with diagnostic messages. Add new file to `tests/CMakeLists.txt` if separate.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes compile report tests.

### 2.3 Authoring Verdict

- [x] 2.3.1 Define `AuthoringVerdict` in `src/util/diagnostics/diagnostic_event.hpp` (or a new `authoring_verdict.hpp`): verdict enum (pass, degraded, fail), findings vector (severity + localization + message), convenience factory `from_compile_reports(...)`.
  - **Verify:** `pixi run build -p debug` compiles.

### 2.4 Chain Manifest Generation in ChainBuilder

- [x] 2.4.1 Add optional `DiagnosticSession*` parameter to `ChainBuilder::build()` in `src/render/chain/chain_builder.hpp`. Default to `nullptr`.
  - **Verify:** `pixi run build -p debug` compiles; existing call sites unchanged.

- [x] 2.4.2 Implement chain manifest generation in `ChainBuilder::build()` in `src/render/chain/chain_builder.cpp`: when session pointer is non-null, build `ChainManifest` from the `PresetConfig`, emit it as a diagnostic event with category `authoring` and stage `"manifest"`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 2.4.3 Wire provenance and compile reports into `ChainBuilder::build()`: pass `SourceProvenanceMap*` to preprocessor and `CompileReport*` to shader runtime during per-pass compilation. Emit compile report events and provenance events through the diagnostic session.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 2.4.4 Implement reflection conformance gate in `ChainBuilder::build()`: after shader compilation, if session is non-null and policy is strict, reject passes with empty reflection contracts (emit error-severity event, return error). In compatibility mode, mark pass as degraded and emit warning-severity event.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 2.4.5 Generate authoring verdict in `ChainBuilder::build()`: after all passes are compiled and validated, produce `AuthoringVerdict` from accumulated compile reports and conformance results. Store in the diagnostic session.
  - **Verify:** `pixi run build -p debug` compiles.

### 2.5 Authoring Integration Tests

- [x] 2.5.1 Create `tests/render/test_authoring_validation.cpp` with Catch2 tests exercising the full authoring analysis path: load a valid preset with a diagnostic session using `TestHarnessSink`, verify chain manifest event is emitted, compile report events are emitted per pass, authoring verdict is "pass". Add to `tests/CMakeLists.txt`.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes authoring validation tests.

- [x] 2.5.2 Add authoring validation tests for error cases in `tests/render/test_authoring_validation.cpp`: preset with intentionally broken shader path produces "fail" verdict with error-severity events; test reflection conformance gate in strict mode rejects degraded pass.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes error-case tests.

### 2.6 Phase 2 Gate

- [x] 2.6.1 Run full CI-parity gate: `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`. All pass with zero new warnings.
  - **Verify:** All three commands succeed.


## Phase 3: Runtime Validation and Capture — Binding Ledger, GPU Timestamps, Intermediate Capture

### 3.1 Diagnostic Session in ChainRuntime

- [x] 3.1.1 Add `std::unique_ptr<diagnostics::DiagnosticSession> m_diagnostic_session` member to `ChainRuntime` in `src/render/chain/chain_runtime.hpp`. Add `create_diagnostic_session(diagnostics::DiagnosticPolicy)` and `diagnostic_session()` accessor methods.
  - **Verify:** `pixi run build -p debug` compiles; no behavioral change without session creation.

- [x] 3.1.2 Implement session creation in `ChainRuntime::create_diagnostic_session()` in `src/render/chain/chain_runtime.cpp`. Wire session into `ChainBuilder::build()` during `load_preset()`. Register default `LogSink` when session is created.
  - **Verify:** `pixi run build -p debug` compiles.

### 3.2 Runtime Event Emission in ChainExecutor

- [x] 3.2.1 Add optional `diagnostics::DiagnosticSession*` parameter to `ChainExecutor::record()` in `src/render/chain/chain_executor.hpp`. Default to `nullptr`.
  - **Verify:** `pixi run build -p debug` compiles; existing call sites unchanged.

- [x] 3.2.2 Implement Tier 0 binding plan event emission in `ChainExecutor::bind_pass_textures()` in `src/render/chain/chain_executor.cpp`: when session is non-null, emit a diagnostic event per pass with category `runtime`, stage `"bind"`, containing `BindingEvidence` for each resolved texture binding (resource identity, fallback status, extent, producer pass).
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.2.3 Implement Tier 0 fallback detection in `ChainExecutor::bind_pass_textures()`: when a texture binding resolves via fallback substitution, emit a degradation event (warning in compatibility mode, error in strict mode). Record in the session's degradation ledger.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.2.4 Implement Tier 0 semantic assignment event emission in the semantic injection path of `ChainExecutor::record()`: when session is non-null, emit a diagnostic event per pass with category `runtime`, stage `"semantic"`, containing `SemanticEvidence`. Populate the session's semantic ledger.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.2.5 Implement execution timeline recording in `ChainExecutor::record()`: when session is non-null, record `TimelineEvent` entries for pass start/end, history push, feedback copy via `begin_frame()`/`end_frame()` lifecycle.
  - **Verify:** `pixi run build -p debug` compiles.

### 3.3 Generation-Aware Install in ChainResources

- [x] 3.3.1 Add `uint64_t m_generation_id = 0` member to `ChainResources` in `src/render/chain/chain_resources.hpp`. Add optional `diagnostics::DiagnosticSession*` parameter to `install()`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.3.2 Implement generation tracking in `ChainResources::install()` in `src/render/chain/chain_resources.cpp`: increment `m_generation_id` on each install. When session is non-null, emit an installation event with pass count, generation id, and session identity update.
  - **Verify:** `pixi run build -p debug` compiles.

### 3.4 GPU Timestamp Pool

- [x] 3.4.1 Create `src/util/diagnostics/gpu_timestamp_pool.hpp` and `src/util/diagnostics/gpu_timestamp_pool.cpp` implementing `GpuTimestampPool` per design interface: `create(device, physical_device, max_passes, frames_in_flight)` returning `Result<unique_ptr>`, `reset_frame(cmd, frame_index)`, `write_timestamp(cmd, frame_index, pass_ordinal, is_start)`, `read_results(frame_index)` returning per-pass durations, `is_available()`. Handle unavailable timestamps gracefully (timestampPeriod == 0). Add to `src/util/diagnostics/CMakeLists.txt`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.4.2 Integrate GPU timestamp queries into `ChainExecutor::record()`: when session is non-null and Tier >= 1, call `reset_frame()` at start, `write_timestamp()` before/after each pass recording. After frame completion (async, not during recording), call `read_results()` and populate execution timeline with GPU durations.
  - **Verify:** `pixi run build -p debug` compiles.

### 3.5 Vulkan Debug Labels

- [x] 3.5.1 Add debug label insertion helpers to `ChainExecutor` (or a utility in `src/util/diagnostics/`): `begin_debug_label(cmd, name, color)` and `end_debug_label(cmd)` using `vk::CommandBuffer::beginDebugUtilsLabelEXT` / `endDebugUtilsLabelEXT`. No-op when `VK_EXT_debug_utils` is not available (check via dispatch table).
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.5.2 Insert debug labels in `ChainExecutor::record()` around each pass recording, history push, and feedback copy operations. Label includes pass ordinal and shader name.
  - **Verify:** `pixi run build -p debug` compiles.

### 3.6 Boundary API Extensions

- [x] 3.6.1 Add new status code `GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE` (value 10) to `src/render/chain/api/c/goggles_filter_chain.h`. Add diagnostic mode/policy `#define` constants, `GogglesChainDiagnosticsCreateInfo`, `GogglesChainDiagnosticsSummary`, and `goggles_chain_diagnostic_event_cb` callback type per design.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.6.2 Add C API function declarations to `src/render/chain/api/c/goggles_filter_chain.h`: `goggles_chain_diagnostics_session_create`, `goggles_chain_diagnostics_session_destroy`, `goggles_chain_diagnostics_sink_register`, `goggles_chain_diagnostics_sink_unregister`, `goggles_chain_diagnostics_summary_get`. Add corresponding `typedef` aliases.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.6.3 Implement C API diagnostic functions in `src/render/chain/api/c/goggles_filter_chain.cpp`: delegate to `ChainRuntime` diagnostic session methods. Create a `CallbackSink` adapter wrapping `goggles_chain_diagnostic_event_cb` for sink registration. Return `GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE` when no session exists.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.6.4 Add C++ diagnostic wrapper methods to `src/render/chain/api/cpp/goggles_filter_chain.hpp` and implement in `src/render/chain/api/cpp/goggles_filter_chain.cpp`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 3.6.5 Update `goggles_chain_status_to_string()` in `src/render/chain/api/c/goggles_filter_chain.cpp` to handle the new `GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE` code.
  - **Verify:** `pixi run build -p debug` compiles.

### 3.7 Runtime Integration Tests

- [x] 3.7.1 Add boundary API diagnostic tests to `tests/render/test_filter_chain_c_api_contracts.cpp`: create session with policy, register callback sink, load preset (which triggers authoring events), record frame (which triggers runtime events), query summary, verify counts. Verify `DIAGNOSTICS_NOT_ACTIVE` returned when no session exists.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes C API diagnostic tests.

- [x] 3.7.2 Create `tests/render/test_runtime_diagnostics.cpp` with Catch2 integration tests: load a preset with diagnostic session and `TestHarnessSink`, record one frame, verify binding ledger has entries for each pass, semantic ledger has entries, execution timeline has pass start/end events. Add to `tests/CMakeLists.txt`.
  - **Verify:** `ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` passes runtime diagnostics tests.

### 3.8 Phase 3 Gate

- [x] 3.8.1 Run full CI-parity gate: `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`. All pass with zero new warnings.
  - **Verify:** All three commands succeed.


## Phase 4: Integration, Quality, and Regression Hardening

### 4.1 Image Comparison Extensions

- [x] 4.1.1 Add `structural_similarity` field (double, 0.0-1.0) to `CompareResult` in `tests/visual/image_compare.hpp`. Add boolean parameter `compute_ssim` to `compare_images()` (default `false`) for backward compatibility.
  - **Verify:** `pixi run build -p debug` compiles; existing image compare tests pass unchanged.

- [x] 4.1.2 Implement structural similarity (SSIM) computation in `tests/visual/image_compare.cpp`: when `compute_ssim` is true, compute SSIM over luminance channel and populate `CompareResult::structural_similarity`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 4.1.3 Add region-of-interest overload to `compare_images()` in `tests/visual/image_compare.hpp` and `tests/visual/image_compare.cpp`: accept a `Rect` (x, y, width, height) parameter, compute metrics only within the region.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 4.1.4 Add SSIM and ROI tests to `tests/visual/test_image_compare.cpp`: verify SSIM of identical images is 1.0; verify SSIM of completely different images is < 0.5; verify ROI comparison only counts pixels within the region.
  - **Verify:** `ctest --preset test -R "^image_compare_unit_tests$" --output-on-failure` passes.

### 4.2 Diff Heatmap Generation

- [x] 4.2.1 Add `generate_diff_heatmap(const Image& actual, const Image& reference, const std::filesystem::path& output)` function declaration to `tests/visual/image_compare.hpp`.
  - **Verify:** `pixi run build -p debug` compiles (declaration only).

- [x] 4.2.2 Implement `generate_diff_heatmap()` in `tests/visual/image_compare.cpp`: compute per-pixel absolute difference, map to a perceptually uniform color gradient (blue=0 through red=max), write as PNG.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 4.2.3 Add heatmap test to `tests/visual/test_image_compare.cpp`: generate heatmap from two known images, verify output file exists and has expected dimensions.
  - **Verify:** `ctest --preset test -R "^image_compare_unit_tests$" --output-on-failure` passes.

### 4.3 Intermediate Golden Baselines (Visual Regression Infrastructure)

- [x] 4.3.1 Create `tests/visual/test_intermediate_golden.cpp` with test infrastructure: helper function to run headless pipeline with diagnostic session capturing intermediate pass outputs at specified ordinals. Use `TestHarnessSink` to collect capture events. Register test with `tests/visual/CMakeLists.txt`.
  - **Verify:** `pixi run build -p debug` compiles.

- [x] 4.3.2 Add intermediate golden comparison logic in `tests/visual/test_intermediate_golden.cpp`: compare captured intermediate outputs against golden baselines using `{preset_name}_pass{ordinal}.png` naming. Missing goldens emit Catch2 SKIP. Failures report pass ordinal.
  - **Verify:** `pixi run build -p debug` compiles.

### 4.4 Temporal Golden Baselines

- [x] 4.4.1 Create `tests/visual/test_temporal_golden.cpp` with test infrastructure: multi-frame capture with golden comparison at specified frame indices. Use `{preset_name}_frame{index}.png` naming for final outputs and `{preset_name}_pass{ordinal}_frame{index}.png` for intermediates. Register with `tests/visual/CMakeLists.txt`.
  - **Verify:** `pixi run build -p debug` compiles.

### 4.5 Forensic Capture Compile-Time Gate

- [x] 4.5.1 Add `GOGGLES_DIAGNOSTICS_FORENSIC` preprocessor define support: add CMake option `GOGGLES_DIAGNOSTICS_FORENSIC` (default OFF) in the diagnostics CMakeLists.txt. When enabled, set compile definition on `goggles_diagnostics` and dependent targets. Follow Tracy pattern from `src/util/profiling.hpp`.
  - **Verify:** `pixi run build -p debug` compiles with and without the flag.

- [x] 4.5.2 Create `src/util/diagnostics/forensic.hpp` with Tier 2 macros: `GOGGLES_DIAG_FORENSIC_SCOPE(name)`, `GOGGLES_DIAG_FORENSIC_CAPTURE(session, pass, cmd)`. When `GOGGLES_DIAGNOSTICS_FORENSIC` is not defined, all macros expand to `(void)0`.
  - **Verify:** `pixi run build -p debug` compiles.

### 4.6 Spec Updates and Documentation

- [x] 4.6.1 Update `openspec/changes/filter-chain-diagnostics/specs/diagnostics/spec.md` with any requirement adjustments discovered during implementation (e.g., concrete method signatures, edge cases found).
  - **Verify:** Spec remains valid and all scenarios still match implementation.

- [x] 4.6.2 Update `openspec/changes/filter-chain-diagnostics/specs/render-pipeline/spec.md` to reflect actual parameter signatures added to `ChainBuilder::build()`, `RetroArchPreprocessor::preprocess()`, and `ShaderRuntime::compile_retroarch_shader()`.
  - **Verify:** Spec requirements match implemented signatures.

- [x] 4.6.3 Update `openspec/changes/filter-chain-diagnostics/specs/goggles-filter-chain/spec.md` to reflect the actual boundary API function signatures and status codes.
  - **Verify:** Spec requirements match C API header.

### 4.7 Final Gate

- [x] 4.7.1 Run full CI-parity gate: `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`. All pass with zero new warnings.
  - **Verify:** All three commands succeed.

- [x] 4.7.2 Run `pixi run semgrep` and verify no new policy violations from diagnostic code.
  - **Verify:** Semgrep passes cleanly.

- [x] 4.7.3 Run `pixi run format` and verify no formatting changes needed in new files.
  - **Verify:** `pixi run format` reports no changes.

---

## Summary

| Phase | Tasks | Focus |
|-------|-------|-------|
| Phase 1 | 25 | Foundation: event model, policy, sinks, session, ledgers, manifest, config |
| Phase 2 | 17 | Authoring analysis: provenance, compile reports, conformance gate, verdict |
| Phase 3 | 21 | Runtime: executor instrumentation, GPU timestamps, debug labels, boundary API |
| Phase 4 | 18 | Integration: image metrics, heatmaps, golden infrastructure, forensic gate, specs |
| **Total** | **81** | |

## Implementation Order

Phases are strictly sequential: Phase 2 depends on types and session from Phase 1; Phase 3 depends on authoring pipeline from Phase 2; Phase 4 depends on runtime infrastructure from Phase 3. Within each phase, tasks are ordered by dependency (infrastructure before consumers, types before tests). Each phase ends with a CI-parity gate to catch regressions early.

## Key Risks

- **GPU timestamp availability**: `GpuTimestampPool` must gracefully degrade when `timestampPeriod == 0`. Task 3.4.1 explicitly handles this.
- **Backward compatibility**: All new parameters default to `nullptr`; existing call sites are unmodified. Each phase is independently revertible.
- **Test isolation**: Diagnostic integration tests (3.7.x) require headless Vulkan which may not be available in CI. Mark GPU-dependent tests with appropriate labels and CI disable guards.
- **SSIM implementation complexity**: Task 4.1.2 implements a simplified luminance-only SSIM. Full multi-channel SSIM can be deferred.
