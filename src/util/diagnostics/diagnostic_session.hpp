#pragma once

#include "binding_ledger.hpp"
#include "chain_manifest.hpp"
#include "degradation_ledger.hpp"
#include "diagnostic_event.hpp"
#include "diagnostic_policy.hpp"
#include "diagnostic_sink.hpp"
#include "execution_timeline.hpp"
#include "semantic_ledger.hpp"
#include "session_identity.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace goggles::diagnostics {

using SinkId = uint32_t;

/// @brief Central diagnostic session: collects events, fans out to sinks, owns ledgers.
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

    [[nodiscard]] auto degradation_ledger() const -> const DegradationLedger&;
    [[nodiscard]] auto binding_ledger() const -> const BindingLedger&;
    [[nodiscard]] auto semantic_ledger() const -> const SemanticAssignmentLedger&;
    [[nodiscard]] auto execution_timeline() const -> const ExecutionTimeline&;
    [[nodiscard]] auto chain_manifest() const -> const ChainManifest*;
    [[nodiscard]] auto authoring_verdict() const -> std::optional<AuthoringVerdict>;

    void set_chain_manifest(std::unique_ptr<ChainManifest> manifest);
    void set_authoring_verdict(AuthoringVerdict verdict);

    [[nodiscard]] auto event_count(Severity severity) const -> uint32_t;
    [[nodiscard]] auto event_count(Category category) const -> uint32_t;
    [[nodiscard]] auto current_frame() const -> uint32_t;
    [[nodiscard]] auto events() const -> std::span<const DiagnosticEvent>;

    void record_binding(BindingEntry entry);
    void record_degradation(DegradationEntry entry);
    void record_semantic(SemanticEntry entry);
    void record_timeline(TimelineEvent event);
    void annotate_gpu_duration(TimelineEventType type, uint32_t pass_ordinal,
                               double gpu_duration_us);

    void begin_frame(uint32_t frame_index);
    void end_frame();
    void reset();

private:
    DiagnosticSession() = default;

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
    uint32_t m_current_frame = 0;
    bool m_has_frame_range = false;
    std::vector<DiagnosticEvent> m_events;
};

} // namespace goggles::diagnostics
