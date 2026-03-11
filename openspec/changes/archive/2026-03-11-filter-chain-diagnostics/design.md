# Design: Filter Chain Diagnostics and Debugging System

## Technical Approach

The diagnostics system introduces a layered observability architecture over the existing filter chain runtime without modifying the core rendering data path. The strategy is to instrument existing boundaries — preset loading, shader compilation, pass recording, texture binding, and temporal operations — rather than restructure them.

The core pattern is a **sink-agnostic event bus** that decouples diagnostic observation from consumption. Every instrumentation point emits a structured `DiagnosticEvent` into a `DiagnosticSession`. The session fans out events to registered sink adapters (log, test-harness, UI, capture). A policy object governs which events are promoted, suppressed, or trigger capture.

This maps directly to the proposal's five-layer architecture:
- **Layer A (Policy & Session)**: `DiagnosticSession` + `DiagnosticPolicy` in `src/util/diagnostics/`
- **Layer B (Authoring Analysis)**: Instrumentation in `RetroArchPreprocessor`, `ShaderRuntime`, `ChainBuilder`
- **Layer C (Runtime Validation)**: Instrumentation in `ChainExecutor::record()`, `ChainExecutor::bind_pass_textures()`, `ChainResources::install()`
- **Layer D (Capture & Inspection)**: New `CaptureManager` integrated with `ChainExecutor` via readback staging buffers
- **Layer E (Quality Validation)**: Extensions to `tests/visual/image_compare.hpp` and new golden harness infrastructure

The activation model uses the existing preprocessor-gated pattern from Tracy (`TRACY_ENABLE` → `GOGGLES_DIAGNOSTICS_FORENSIC`) for Tier 2. Tier 0 and Tier 1 are runtime-toggled through `DiagnosticPolicy`.

## Architecture Decisions

### Decision: Place Diagnostic Core in `src/util/diagnostics/`

**Choice**: New `src/util/diagnostics/` module for event model, session, policy, sinks, and ledgers.
**Alternatives considered**: (a) Scatter diagnostic types across existing modules; (b) Create a top-level `src/diagnostics/` peer to `src/render/`.
**Rationale**: Diagnostics are cross-cutting infrastructure consumed by `render/chain/`, `render/shader/`, `render/backend/`, and `tests/`. The `util/` namespace already holds logging, profiling, error handling, and config — all infrastructure peers. A dedicated subdirectory keeps the diagnostic surface organized without creating a new top-level module. Option (a) would fragment the event model; option (b) would break the established convention that infrastructure lives under `util/`.

### Decision: Single `DiagnosticEvent` Type With Variant Evidence Payload

**Choice**: One `DiagnosticEvent` struct with severity, category, localization key, session reference, and a `std::variant`-based evidence payload.
**Alternatives considered**: (a) Event class hierarchy with virtual methods; (b) Completely untyped `std::any` payload.
**Rationale**: A single concrete type avoids virtual dispatch overhead on the hot path (Tier 0 checks run every frame). `std::variant` provides compile-time type safety for evidence payloads while keeping the event type uniform for sink dispatch. A class hierarchy would bloat the vtable and complicate sink implementations. `std::any` would lose type safety at consumption sites.

### Decision: Sink Adapter as Single-Method Interface

**Choice**: `DiagnosticSink` abstract base with one pure virtual method: `void receive(const DiagnosticEvent& event)`.
**Alternatives considered**: (a) Callback-based (`std::function<void(const DiagnosticEvent&)>`); (b) Multi-method interface with per-category receivers.
**Rationale**: A single-method interface is the narrowest possible contract. It makes sink implementations trivial (log sink: format and write; test sink: push to vector). The codebase uses abstract bases with factory functions returning `ResultPtr<T>` — this follows that pattern. Callbacks would obscure ownership and make sink lifecycle management harder. Per-category methods would couple the interface to the category enum.

### Decision: DiagnosticSession Owns Ledgers, Not Sinks

**Choice**: The `DiagnosticSession` owns the degradation ledger, binding ledger, semantic ledger, and chain manifest as in-memory structures. Sinks receive events but do not own state.
**Alternatives considered**: Sinks accumulate their own ledger state.
**Rationale**: Ledgers must be queryable by the boundary API (`goggles_chain_*` functions) without knowing which sinks are registered. Centralizing state in the session makes the C API straightforward: `goggles_chain_diagnostics_*` functions read from the session. Sinks are output-only channels.

### Decision: GPU Timestamp Queries via Dedicated Query Pool

**Choice**: Allocate a `vk::QueryPool` with `vk::QueryType::eTimestamp` sized to `2 * (max_passes + 2)` per frame-in-flight. Reset and write queries during command recording. Read back results after fence signal using `vk::Device::getQueryPoolResults` with `vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait`.
**Alternatives considered**: (a) Use Tracy's GPU timing infrastructure directly; (b) Use `VK_EXT_host_query_reset` for simpler reset.
**Rationale**: Tracy GPU zones are compile-time gated and require Tracy to be enabled. Tier 1 GPU timestamps must work independently of Tracy. A dedicated query pool gives full control over readback timing and avoids coupling to Tracy's instrumentation model. `VK_EXT_host_query_reset` is not universally available; `vkCmdResetQueryPool` inside the command buffer is the safe fallback. The query pool is allocated once and reused, so the overhead is allocation-time only.

### Decision: Readback Staging Uses Existing Framebuffer Allocation Pattern

**Choice**: Readback staging buffers use the same `vk::Buffer` + `vk::DeviceMemory` allocation pattern as the existing `FilterPass::create_ubo_buffer()`, with `vk::BufferUsageFlagBits::eTransferDst` and host-visible + host-coherent memory.
**Alternatives considered**: (a) Introduce a VMA-based allocator; (b) Use persistent mapped buffers.
**Rationale**: The codebase does not use VMA — every allocation goes through `vk::Device::allocateMemory` with manual memory type selection via `find_memory_type`. Introducing VMA would be a separate, larger refactor. The existing pattern is proven and consistent. Persistent mapping is fine for staging buffers and already used for UBO buffers.

### Decision: Tier 2 Compile-Time Gate Follows Tracy's Pattern

**Choice**: `GOGGLES_DIAGNOSTICS_FORENSIC` preprocessor define enables Tier 2 instrumentation. When undefined, Tier 2 code paths are compiled out entirely via `#ifdef` guards and no-op macros.
**Alternatives considered**: Runtime `if constexpr` with a template parameter; always-compiled with runtime disable.
**Rationale**: The codebase already uses `TRACY_ENABLE` for the same purpose (see `src/util/profiling.hpp`). Developers expect this pattern. `if constexpr` would require template parameterization across the recording hot path, which would be invasive. Always-compiled code would violate the zero-overhead requirement for Tier 2.

### Decision: Boundary API Extensions Use the Existing `goggles_chain_` C API Pattern

**Choice**: New diagnostic functions follow the same pattern: `goggles_chain_diagnostics_session_create(...)`, `goggles_chain_diagnostics_sink_register(...)`, etc. All return `goggles_chain_status_t`. New struct types use `struct_size` versioning.
**Alternatives considered**: A separate `goggles_diagnostics_` function prefix with its own opaque handle.
**Rationale**: The diagnostic session is scoped to a chain runtime instance. Separating the API would require passing both a chain handle and a diagnostics handle, complicating the host contract. The existing C API already has `goggles_chain_error_last_info_get` as a diagnostic precedent. Extending the existing prefix maintains a single namespace for host code.

### Decision: TOML Configuration Under `[diagnostics]` Section

**Choice**: Add a `[diagnostics]` section to the shipped template at `config/goggles.template.toml`, have first-run bootstrap copy it to `${XDG_CONFIG_HOME:-$HOME/.config}/goggles/goggles.toml` when the runtime user config is missing, and extend `goggles::Config` with a `Diagnostics` member.
**Alternatives considered**: Separate diagnostics config file; environment variables only.
**Rationale**: The application already loads all configuration from a single TOML file via `goggles::load_config()`. Adding a section follows the established pattern. Environment variables are appropriate for CI overrides but should not be the primary configuration mechanism. A separate file would fragment configuration.

## Data Flow

### Diagnostic Event Flow

```
  Instrumentation Points                 DiagnosticSession                    Sinks
  =====================                 ==================                   =====

  RetroArchPreprocessor ─┐
  ShaderRuntime ─────────┤
  ChainBuilder ──────────┤              ┌──────────────────┐
  ChainExecutor ─────────┼── emit() ──→│  DiagnosticSession │──→ LogSink (spdlog)
  ChainResources ────────┤              │                    │──→ TestHarnessSink
  CaptureManager ────────┘              │  ┌─ policy ──────┐│──→ TracySink (Tier 2)
                                        │  │ strict/compat  ││──→ ImGuiSink (future)
                                        │  │ capture mode   ││──→ CaptureSink (future)
                                        │  │ tier level     ││
                                        │  └────────────────┘│
                                        │  ┌─ ledgers ─────┐│
                                        │  │ degradation    ││
                                        │  │ binding        ││
                                        │  │ semantic       ││
                                        │  │ execution tl   ││
                                        │  │ chain manifest ││
                                        │  └────────────────┘│
                                        └──────────────────────┘
                                                 │
                                                 ▼
                                        Boundary C API
                                        goggles_chain_diagnostics_*
```

### Per-Frame Recording with Diagnostics

```
  ChainRuntime::record()
       │
       ├─ ChainExecutor::record()
       │       │
       │       ├─ [Tier 1] reset GPU timestamp query pool
       │       │
       │       ├─ ensure_prechain_passes()
       │       │       └─ emit: allocation event
       │       │
       │       ├─ record_prechain()
       │       │       ├─ [Tier 1] write timestamp (start)
       │       │       ├─ pass->record(cmd, ctx)
       │       │       └─ [Tier 1] write timestamp (end)
       │       │
       │       ├─ for each effect pass:
       │       │   ├─ set_source_size / set_output_size / ... (semantic injection)
       │       │   │       └─ [Tier 0] emit: semantic assignment event
       │       │   │
       │       │   ├─ bind_pass_textures()
       │       │   │       ├─ [Tier 0] emit: binding plan event per pass
       │       │   │       └─ [Tier 0] on fallback: emit degradation event
       │       │   │
       │       │   ├─ [Tier 1] write timestamp (start)
       │       │   ├─ [debug labels] vkCmdBeginDebugUtilsLabelEXT
       │       │   ├─ pass->record(cmd, ctx)
       │       │   ├─ [debug labels] vkCmdEndDebugUtilsLabelEXT
       │       │   ├─ [Tier 1] write timestamp (end)
       │       │   │
       │       │   └─ [Tier 1+] optional intermediate readback copy
       │       │
       │       ├─ record_postchain()
       │       │
       │       ├─ frame_history.push()
       │       │       └─ [Tier 0] emit: history push event
       │       │
       │       └─ copy_feedback_framebuffers()
       │               └─ [Tier 0] emit: feedback copy event
       │
       └─ [Tier 1] async readback: GPU timestamps + captured images
```

### Authoring Analysis Flow (Preset Load)

```
  ChainRuntime::load_preset()
       │
       ├─ PresetParser::load()
       │       └─ emit: preset parse event (normalized structure, redirect graph)
       │
       ├─ For each pass:
       │   ├─ RetroArchPreprocessor::preprocess()
       │   │       ├─ resolve_includes()
       │   │       │       └─ emit: include graph event + source provenance
       │   │       ├─ extract_parameters()
       │   │       └─ split_by_stage()
       │   │
       │   ├─ ShaderRuntime::compile_retroarch_shader()
       │   │       └─ emit: compile report event (per-stage, timing, cache-hit)
       │   │
       │   └─ merge_reflection()
       │           └─ emit: reflection report event (merged contract)
       │
       ├─ ChainBuilder::build()
       │       ├─ validate graph topology
       │       ├─ validate temporal requirements
       │       └─ emit: chain manifest event
       │
       ├─ [Strict mode] Reflection conformance gate
       │       └─ emit: conformance verdict event
       │
       └─ ChainResources::install()
               ├─ emit: installation event (pass count, generation id)
               └─ update session identity hashes
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/util/diagnostics/diagnostic_event.hpp` | Create | Diagnostic event model: `Severity`, `Category`, `LocalizationKey`, `DiagnosticEvent`, evidence payload variants |
| `src/util/diagnostics/diagnostic_session.hpp` | Create | `DiagnosticSession`: event emission, sink registry, ledger ownership, session identity |
| `src/util/diagnostics/diagnostic_session.cpp` | Create | Session implementation: fan-out to sinks, ledger population, policy evaluation |
| `src/util/diagnostics/diagnostic_policy.hpp` | Create | `DiagnosticPolicy`: strict/compat mode, tier level, capture mode, severity promotion rules |
| `src/util/diagnostics/diagnostic_sink.hpp` | Create | `DiagnosticSink` abstract interface |
| `src/util/diagnostics/log_sink.hpp` | Create | `LogSink`: formats events and routes to spdlog with dedicated logger name |
| `src/util/diagnostics/log_sink.cpp` | Create | LogSink implementation |
| `src/util/diagnostics/test_harness_sink.hpp` | Create | `TestHarnessSink`: collects events in queryable vector, supports filter-by-category/severity |
| `src/util/diagnostics/test_harness_sink.cpp` | Create | TestHarnessSink implementation |
| `src/util/diagnostics/degradation_ledger.hpp` | Create | `DegradationLedger`: records fallback substitutions, unresolved semantics, reflection loss |
| `src/util/diagnostics/binding_ledger.hpp` | Create | `BindingLedger`: per-pass, per-frame resolved resource table |
| `src/util/diagnostics/semantic_ledger.hpp` | Create | `SemanticAssignmentLedger`: per-pass semantic destination classification and values |
| `src/util/diagnostics/execution_timeline.hpp` | Create | `ExecutionTimeline`: ordered event list with optional GPU/CPU timestamps |
| `src/util/diagnostics/chain_manifest.hpp` | Create | `ChainManifest`: normalized preset description (passes, aliases, textures, temporal) |
| `src/util/diagnostics/source_provenance.hpp` | Create | `SourceProvenanceMap`: expanded-line to original-file-and-line mapping |
| `src/util/diagnostics/compile_report.hpp` | Create | `CompileReport`: per-stage success, diagnostics, timing, cache status |
| `src/util/diagnostics/session_identity.hpp` | Create | `SessionIdentity`: content hashes, generation id, environment fingerprint |
| `src/util/diagnostics/gpu_timestamp_pool.hpp` | Create | `GpuTimestampPool`: Vulkan query pool wrapper for per-pass GPU timing |
| `src/util/diagnostics/gpu_timestamp_pool.cpp` | Create | GpuTimestampPool implementation |
| `src/util/diagnostics/CMakeLists.txt` | Create | Build target for diagnostics module |
| `src/util/config.hpp` | Modify | Add `Config::Diagnostics` struct with mode, tier, strict toggle, capture depth, retention |
| `src/util/config.cpp` | Modify | Parse `[diagnostics]` TOML section |
| `src/render/chain/chain_runtime.hpp` | Modify | Add `std::unique_ptr<DiagnosticSession> m_diagnostic_session` member; add diagnostic session lifecycle methods |
| `src/render/chain/chain_runtime.cpp` | Modify | Create `DiagnosticSession` during `ChainRuntime::create()`; wire session into executor and builder |
| `src/render/chain/chain_executor.hpp` | Modify | Accept optional `DiagnosticSession*` in `record()` signature |
| `src/render/chain/chain_executor.cpp` | Modify | Add Tier 0 event emission in `bind_pass_textures()` for binding plan, fallback detection, semantic population; Tier 1 GPU timestamp writes; debug label insertion |
| `src/render/chain/chain_builder.hpp` | Modify | Accept optional `DiagnosticSession*` in `build()` factory |
| `src/render/chain/chain_builder.cpp` | Modify | Emit chain manifest, conformance gate events; pass session to shader compilation |
| `src/render/chain/chain_resources.hpp` | Modify | Add generation id (`uint64_t m_generation_id`); accept optional `DiagnosticSession*` in `install()` |
| `src/render/chain/chain_resources.cpp` | Modify | Emit installation event; increment generation id on install |
| `src/render/shader/retroarch_preprocessor.hpp` | Modify | Add optional `SourceProvenanceMap*` output parameter to `preprocess()` and `preprocess_source()` |
| `src/render/shader/retroarch_preprocessor.cpp` | Modify | Build provenance map during `resolve_includes()` and compatibility rewrites |
| `src/render/shader/shader_runtime.hpp` | Modify | Add optional `CompileReport*` output parameter to `compile_retroarch_shader()` |
| `src/render/shader/shader_runtime.cpp` | Modify | Populate compile report with per-stage timing, diagnostics, cache-hit state |
| `src/render/chain/api/c/goggles_filter_chain.h` | Modify | Add `goggles_chain_diagnostics_*` function declarations, `GogglesChainDiagnosticsCreateInfo`, `GogglesChainDiagnosticsEventCallback` types |
| `src/render/chain/api/c/goggles_filter_chain.cpp` | Modify | Implement C API diagnostic functions delegating to `ChainRuntime` |
| `src/render/chain/api/cpp/goggles_filter_chain.hpp` | Modify | Add C++ diagnostic session wrapper methods |
| `src/render/chain/api/cpp/goggles_filter_chain.cpp` | Modify | Implement C++ diagnostic methods |
| `config/goggles.template.toml` | Modify | Add `[diagnostics]` section with defaults; first-run bootstrap copies the template to `${XDG_CONFIG_HOME:-$HOME/.config}/goggles/goggles.toml` when needed |
| `tests/render/test_diagnostic_event_model.cpp` | Create | Unit tests for event model, severity ordering, policy promotion |
| `tests/render/test_diagnostic_session.cpp` | Create | Unit tests for session lifecycle, multi-sink fan-out, ledger population |
| `tests/render/test_diagnostic_sinks.cpp` | Create | Unit tests for log sink formatting, test-harness sink collection and filtering |
| `tests/render/test_authoring_validation.cpp` | Create | Tests for static validation: compile report emission, reflection conformance, provenance |
| `tests/render/test_binding_ledger.cpp` | Create | Tests for binding plan recording, fallback detection, producer chain tracking |
| `tests/visual/test_intermediate_golden.cpp` | Create | Tests for per-pass intermediate golden comparisons |
| `tests/visual/test_temporal_golden.cpp` | Create | Tests for multi-frame temporal sequence golden comparisons |
| `tests/visual/test_diff_heatmap.cpp` | Create | Tests for diff heatmap generation |
| `tests/visual/image_compare.hpp` | Modify | Add `structural_similarity` field to `CompareResult`; add region-of-interest overload |
| `tests/visual/image_compare.cpp` | Modify | Implement structural similarity metric; implement ROI comparison |

## Interfaces / Contracts

### Core Event Model (`src/util/diagnostics/diagnostic_event.hpp`)

```cpp
namespace goggles::diagnostics {

enum class Severity : uint8_t { debug, info, warning, error };

enum class Category : uint8_t { authoring, runtime, quality, capture };

struct LocalizationKey {
    static constexpr uint32_t CHAIN_LEVEL = UINT32_MAX;
    uint32_t pass_ordinal = CHAIN_LEVEL;
    std::string_view stage;       // e.g., "compile", "bind", "record", "preset_parse"
    std::string_view resource;    // e.g., texture name, semantic name, or empty
};

struct SessionIdentity {
    std::string preset_hash;
    std::string expanded_source_hash;
    std::string compiled_contract_hash;
    uint64_t generation_id = 0;
    uint32_t frame_start = 0;
    uint32_t frame_end = 0;
    std::string capture_mode;       // "minimal", "standard", "investigate", "forensic"
    std::string environment_fingerprint;
};

// Evidence payload variants — each category defines its own evidence types
struct BindingEvidence { /* resource id, fallback status, extent, producer pass */ };
struct SemanticEvidence { /* member name, classification, value, offset */ };
struct CompileEvidence { /* stage, success, messages, timing_us, cache_hit */ };
struct ReflectionEvidence { /* stage, resource summary, merge conflicts */ };
struct ProvenanceEvidence { /* original file, original line, rewrite applied */ };
struct CaptureEvidence { /* image data reference, pass ordinal, frame index */ };

using EvidencePayload = std::variant<
    std::monostate,
    BindingEvidence,
    SemanticEvidence,
    CompileEvidence,
    ReflectionEvidence,
    ProvenanceEvidence,
    CaptureEvidence
>;

struct DiagnosticEvent {
    Severity severity;
    Severity original_severity;   // before policy promotion
    Category category;
    LocalizationKey localization;
    uint32_t frame_index = 0;
    uint64_t timestamp_ns = 0;
    std::string message;
    EvidencePayload evidence;
    // Session identity is carried by the session, not duplicated per event
};

} // namespace goggles::diagnostics
```

### Sink Interface (`src/util/diagnostics/diagnostic_sink.hpp`)

```cpp
namespace goggles::diagnostics {

class DiagnosticSink {
public:
    virtual ~DiagnosticSink() = default;
    virtual void receive(const DiagnosticEvent& event) = 0;
};

} // namespace goggles::diagnostics
```

### Diagnostic Session (`src/util/diagnostics/diagnostic_session.hpp`)

```cpp
namespace goggles::diagnostics {

using SinkId = uint32_t;

class DiagnosticSession {
public:
    [[nodiscard]] static auto create(DiagnosticPolicy policy) -> std::unique_ptr<DiagnosticSession>;

    void emit(DiagnosticEvent event);

    auto register_sink(std::unique_ptr<DiagnosticSink> sink) -> SinkId;
    void unregister_sink(SinkId id);

    [[nodiscard]] auto policy() const -> const DiagnosticPolicy&;
    void set_policy(DiagnosticPolicy policy);

    [[nodiscard]] auto identity() const -> const SessionIdentity&;
    void update_identity(SessionIdentity identity);

    // Ledger access for boundary API
    [[nodiscard]] auto degradation_ledger() const -> const DegradationLedger&;
    [[nodiscard]] auto binding_ledger() const -> const BindingLedger&;
    [[nodiscard]] auto semantic_ledger() const -> const SemanticAssignmentLedger&;
    [[nodiscard]] auto execution_timeline() const -> const ExecutionTimeline&;
    [[nodiscard]] auto chain_manifest() const -> const ChainManifest*;
    [[nodiscard]] auto authoring_verdict() const -> std::optional<AuthoringVerdict>;

    // Aggregate counts for quick queries
    [[nodiscard]] auto event_count(Severity severity) const -> uint32_t;
    [[nodiscard]] auto event_count(Category category) const -> uint32_t;

    void begin_frame(uint32_t frame_index);
    void end_frame();
    void reset();

private:
    DiagnosticPolicy m_policy;
    SessionIdentity m_identity;
    std::vector<std::pair<SinkId, std::unique_ptr<DiagnosticSink>>> m_sinks;
    SinkId m_next_sink_id = 0;

    DegradationLedger m_degradation_ledger;
    BindingLedger m_binding_ledger;
    SemanticAssignmentLedger m_semantic_ledger;
    ExecutionTimeline m_timeline;
    std::unique_ptr<ChainManifest> m_manifest;
    std::optional<AuthoringVerdict> m_verdict;

    std::array<uint32_t, 4> m_severity_counts{};
    std::array<uint32_t, 4> m_category_counts{};
};

} // namespace goggles::diagnostics
```

### Diagnostic Policy (`src/util/diagnostics/diagnostic_policy.hpp`)

```cpp
namespace goggles::diagnostics {

enum class PolicyMode : uint8_t { compatibility, strict };
enum class CaptureMode : uint8_t { minimal, standard, investigate, forensic };
enum class ActivationTier : uint8_t { tier0, tier1, tier2 };

struct DiagnosticPolicy {
    PolicyMode mode = PolicyMode::compatibility;
    CaptureMode capture_mode = CaptureMode::standard;
    ActivationTier tier = ActivationTier::tier0;
    uint32_t capture_frame_limit = 1;
    uint64_t retention_bytes = 256 * 1024 * 1024; // 256 MB
    bool promote_fallback_to_error = false;        // derived from mode == strict
    bool reflection_loss_is_fatal = false;         // derived from mode == strict
};

} // namespace goggles::diagnostics
```

### Boundary API Extension (`src/render/chain/api/c/goggles_filter_chain.h`)

```c
// New status code
#define GOGGLES_CHAIN_STATUS_DIAGNOSTICS_NOT_ACTIVE ((goggles_chain_status_t)10u)

// New diagnostic reporting modes
#define GOGGLES_CHAIN_DIAG_MODE_MINIMAL    ((uint32_t)0u)
#define GOGGLES_CHAIN_DIAG_MODE_STANDARD   ((uint32_t)1u)
#define GOGGLES_CHAIN_DIAG_MODE_INVESTIGATE ((uint32_t)2u)
#define GOGGLES_CHAIN_DIAG_MODE_FORENSIC   ((uint32_t)3u)

// New policy modes
#define GOGGLES_CHAIN_DIAG_POLICY_COMPATIBILITY ((uint32_t)0u)
#define GOGGLES_CHAIN_DIAG_POLICY_STRICT        ((uint32_t)1u)

struct GogglesChainDiagnosticsCreateInfo {
    uint32_t struct_size;
    uint32_t reporting_mode;
    uint32_t policy_mode;
    uint32_t activation_tier;   // 0, 1, or 2
};

struct GogglesChainDiagnosticsSummary {
    uint32_t struct_size;
    uint32_t reporting_mode;
    uint32_t policy_mode;
    uint32_t error_count;
    uint32_t warning_count;
    uint32_t info_count;
};

// Callback type for sink adapter via C boundary
typedef void(GOGGLES_CHAIN_CALL* goggles_chain_diagnostic_event_cb)(
    uint32_t severity, uint32_t category, uint32_t pass_ordinal,
    const char* message_utf8, void* user_data);

// Session lifecycle
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_diagnostics_session_create(
    goggles_chain_t* chain,
    const GogglesChainDiagnosticsCreateInfo* create_info);

GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_diagnostics_session_destroy(goggles_chain_t* chain);

// Sink registration
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_diagnostics_sink_register(
    goggles_chain_t* chain,
    goggles_chain_diagnostic_event_cb callback,
    void* user_data,
    uint32_t* out_sink_id);

GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_diagnostics_sink_unregister(
    goggles_chain_t* chain,
    uint32_t sink_id);

// State queries
GOGGLES_CHAIN_API goggles_chain_status_t GOGGLES_CHAIN_CALL
goggles_chain_diagnostics_summary_get(
    const goggles_chain_t* chain,
    GogglesChainDiagnosticsSummary* out_summary);
```

### GPU Timestamp Pool (`src/util/diagnostics/gpu_timestamp_pool.hpp`)

```cpp
namespace goggles::diagnostics {

class GpuTimestampPool {
public:
    [[nodiscard]] static auto create(vk::Device device, vk::PhysicalDevice physical_device,
                                      uint32_t max_passes, uint32_t frames_in_flight)
        -> Result<std::unique_ptr<GpuTimestampPool>>;

    void reset_frame(vk::CommandBuffer cmd, uint32_t frame_index);
    void write_timestamp(vk::CommandBuffer cmd, uint32_t frame_index,
                         uint32_t pass_ordinal, bool is_start);
    [[nodiscard]] auto read_results(uint32_t frame_index)
        -> Result<std::vector<std::pair<uint32_t, double>>>;  // pass_ordinal -> duration_us

    [[nodiscard]] auto is_available() const -> bool;

private:
    vk::Device m_device;
    vk::QueryPool m_pool;
    float m_timestamp_period = 0.0f;
    uint32_t m_queries_per_frame = 0;
    uint32_t m_frames_in_flight = 0;
    bool m_available = false;
};

} // namespace goggles::diagnostics
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | Event model severity ordering, policy promotion rules, localization key construction | Pure C++ unit tests in `tests/render/test_diagnostic_event_model.cpp` with Catch2 |
| Unit | DiagnosticSession: multi-sink fan-out, sink failure isolation, event counting, ledger population | Unit tests with mock sinks; verify event delivery order and counts |
| Unit | TestHarnessSink: collection, filter by severity/category, emission order preservation | Direct sink instantiation and assertion on collected events |
| Unit | LogSink: verify formatted output routes to spdlog dedicated logger | Capture spdlog output and verify format |
| Unit | DegradationLedger: entry creation, query by pass ordinal, frame ordering | Populate ledger with known entries, query and verify |
| Unit | BindingLedger: resolved/substituted/unresolved classification, extent recording | Populate and query binding entries per pass |
| Unit | SemanticAssignmentLedger: destination classification, value recording, alias resolution | Populate and query semantic entries |
| Unit | ChainManifest: deterministic generation from PresetConfig, temporal requirement extraction | Compare manifests from same preset, verify byte-identical |
| Unit | SourceProvenanceMap: line mapping through includes, rewrite tracking | Preprocess known shader sources, verify provenance entries |
| Unit | CompileReport: per-stage recording, cache-hit tracking, timing | Compile known shaders, verify report fields |
| Unit | DiagnosticPolicy: strict mode severity promotion, tier gating | Create policies, verify event transformation |
| Unit | GpuTimestampPool: creation with/without timestamp support, availability detection | Test with mock device properties |
| Integration | Authoring validation end-to-end: load preset, verify compile report + reflection report + conformance | Use existing test preset corpus; verify diagnostic events via test-harness sink |
| Integration | Runtime validation: load preset, record frame, verify binding ledger + degradation ledger | Headless recording with test-harness sink; assert on ledger contents |
| Integration | Boundary API: create session, register sink, load preset, record, query summary | C API tests extending existing `test_filter_chain_c_api_contracts.cpp` |
| Integration | GPU timestamps: record frame with Tier 1, verify timeline has per-pass timing | Headless integration with real Vulkan device |
| E2E | Intermediate golden workflow: capture per-pass outputs, compare against baselines | Extend existing visual regression suite |
| E2E | Temporal golden workflow: multi-frame capture, verify history/feedback progression | New temporal golden test presets |
| E2E | Authoring corpus: invalid presets produce expected diagnostic verdicts | Batch test with intentionally invalid presets |

## Migration / Rollout

### Phase 1: Contract Visibility (Foundation)

**Deliverables:**
- `src/util/diagnostics/` module with event model, session, policy, sinks (log + test-harness)
- Chain manifest generation in `ChainBuilder`
- Source provenance tracking in `RetroArchPreprocessor`
- Compile and reflection reports in `ShaderRuntime`
- Reflection conformance gate (strict mode: reject; compat mode: degrade + record)
- Parameter and semantic conformance validation
- Authoring verdict generation
- `[diagnostics]` config section
- Unit tests for all new types
- Authoring validation integration tests

**Integration points:** `ChainBuilder::build()` gains optional `DiagnosticSession*` parameter. `RetroArchPreprocessor::preprocess()` gains optional `SourceProvenanceMap*` output. `ShaderRuntime::compile_retroarch_shader()` gains optional `CompileReport*` output. All existing call sites pass `nullptr` by default — zero behavioral change without a diagnostic session.

**Revertible:** Remove `src/util/diagnostics/` and revert parameter additions to factory functions.

### Phase 2: Runtime Truth

**Deliverables:**
- Binding ledger population in `ChainExecutor::bind_pass_textures()`
- Degradation ledger population (fallback detection at bind time)
- Semantic assignment ledger in semantic injection path
- Execution timeline with CPU timestamps
- Generation-aware rebuild/swap reporting in `ChainResources::install()`
- Boundary C API diagnostic session lifecycle + sink registration + summary query
- Diagnostic session member on `ChainRuntime`
- Runtime validation integration tests

**Integration points:** `ChainExecutor::record()` gains optional `DiagnosticSession*`. `ChainResources::install()` increments generation id and emits installation event. `goggles_filter_chain.h` gets new function declarations.

**Revertible:** Revert `ChainExecutor` and `ChainResources` instrumentation; remove C API additions.

### Phase 3: Image and Temporal Evidence

**Deliverables:**
- `GpuTimestampPool` for per-pass GPU timing
- Vulkan debug label insertion at pass boundaries (`VK_EXT_debug_utils`)
- Intermediate output readback via staging buffers (on-demand capture)
- History and feedback surface capture
- Capture control through boundary API
- Diff heatmap generation in `tests/visual/`
- Structural similarity metric in image comparison library
- Region-of-interest comparison
- Intermediate golden workflow

**Integration points:** `GpuTimestampPool` allocated in `ChainRuntime::create()` when Tier 1 active. Readback staging allocated per-capture-request. Debug labels conditional on extension availability.

**Revertible:** Remove GPU query pool, staging buffers, debug labels; revert image comparison extensions.

### Phase 4: Regression Hardening

**Deliverables:**
- Authoring validation corpus (intentionally invalid presets)
- Semantic-probe presets (size, frame counter, parameter isolation)
- Temporal golden baselines for history and feedback
- Per-pass intermediate golden baselines
- Earliest-divergence localization in visual regression
- Tiered CI/release workflow configuration
- Forensic capture (Tier 2, compile-time gated)

**Revertible:** Remove test corpus additions and golden baselines; revert Tier 2 compile flag.

### Migration Notes

- No data migration required. The diagnostics system adds new artifacts alongside existing behavior.
- No feature flags beyond the `[diagnostics]` TOML config and `GOGGLES_DIAGNOSTICS_FORENSIC` compile flag.
- Existing tests are unaffected: all diagnostic parameters default to `nullptr` or inactive state.
- Strict mode is opt-in. CI workflows can enable it progressively once the authoring corpus is clean.

## Open Questions

- [ ] Should the `DiagnosticEvent` use `std::string` or `std::string_view` for the message field? String views are cheaper but require the underlying storage to outlive the event. If events are consumed synchronously by sinks before the frame ends, `string_view` is safe. If events are buffered for async serialization, `std::string` is needed. **Recommendation:** Use `std::string` for safety; optimize to `string_view` with arena allocation in Phase 3 if profiling shows allocation pressure.
- [ ] Should intermediate readback (Phase 3) use a ring of staging buffers or allocate per-capture? A ring avoids repeated allocation but consumes persistent memory. **Recommendation:** Start with per-capture allocation; add ring pooling if capture frequency warrants it.
- [ ] What is the maximum number of passes we need to support for GPU timestamp queries? The current `max_passes` is implicitly unbounded. **Recommendation:** Cap at 64 passes for initial query pool sizing; emit a diagnostic warning if a preset exceeds this.
- [ ] Should the `ChainManifest` be serializable to JSON for external tooling? **Recommendation:** Yes, add JSON serialization in Phase 2 alongside the boundary API, using the project's existing serializer pattern if one exists or a lightweight approach.
