#include "diagnostic_session.hpp"

#include "support/logging.hpp"

#include <algorithm>

namespace goggles::diagnostics {

namespace {

auto emit_sink_failure_event(DiagnosticSession& session, SinkId sink_id) -> void {
    DiagnosticEvent event{};
    event.severity = Severity::warning;
    event.original_severity = Severity::warning;
    event.category = Category::runtime;
    event.localization = {
        .pass_ordinal = LocalizationKey::CHAIN_LEVEL, .stage = "sink", .resource = {}};
    event.message = std::string("Diagnostic sink ") + std::to_string(sink_id) +
                    " failed while receiving an event";
    session.emit(std::move(event));
}

} // namespace

auto DiagnosticSession::create(DiagnosticPolicy policy) -> std::unique_ptr<DiagnosticSession> {
    auto session = std::unique_ptr<DiagnosticSession>(new DiagnosticSession());
    session->m_policy = policy;
    return session;
}

void DiagnosticSession::emit(DiagnosticEvent event) {
    event.original_severity = event.severity;
    event.session_identity = m_identity;

    if (m_policy.promote_fallback_to_error && event.severity == Severity::warning &&
        event.category == Category::runtime) {
        if (std::holds_alternative<BindingEvidence>(event.evidence)) {
            const auto& binding = std::get<BindingEvidence>(event.evidence);
            if (binding.is_fallback) {
                event.severity = Severity::error;
            }
        }
    }

    auto sev_idx = static_cast<size_t>(event.severity);
    auto cat_idx = static_cast<size_t>(event.category);
    if (sev_idx < m_severity_counts.size()) {
        ++m_severity_counts[sev_idx];
    }
    if (cat_idx < m_category_counts.size()) {
        ++m_category_counts[cat_idx];
    }

    m_events.push_back(event);

    std::vector<SinkId> failed_sinks;
    failed_sinks.reserve(m_sinks.size());

    for (auto& [id, sink] : m_sinks) {
        try {
            sink->receive(event);
        } catch (...) {
            GOGGLES_LOG_WARN("Diagnostic sink {} threw during receive", id);
            failed_sinks.push_back(id);
        }
    }

    for (auto failed_sink : failed_sinks) {
        m_sinks.erase(
            std::remove_if(m_sinks.begin(), m_sinks.end(),
                           [failed_sink](const auto& pair) { return pair.first == failed_sink; }),
            m_sinks.end());
        emit_sink_failure_event(*this, failed_sink);
    }
}

auto DiagnosticSession::register_sink(std::unique_ptr<DiagnosticSink> sink) -> SinkId {
    auto id = m_next_sink_id++;
    m_sinks.emplace_back(id, std::move(sink));
    return id;
}

void DiagnosticSession::unregister_sink(SinkId id) {
    m_sinks.erase(std::remove_if(m_sinks.begin(), m_sinks.end(),
                                 [id](const auto& pair) { return pair.first == id; }),
                  m_sinks.end());
}

auto DiagnosticSession::policy() const -> const DiagnosticPolicy& {
    return m_policy;
}

void DiagnosticSession::set_policy(DiagnosticPolicy policy) {
    m_policy = policy;
}

auto DiagnosticSession::identity() const -> const SessionIdentity& {
    return m_identity;
}

void DiagnosticSession::update_identity(SessionIdentity identity) {
    m_identity = std::move(identity);
}

auto DiagnosticSession::degradation_ledger() const -> const DegradationLedger& {
    return m_degradation_ledger;
}

auto DiagnosticSession::binding_ledger() const -> const BindingLedger& {
    return m_binding_ledger;
}

auto DiagnosticSession::semantic_ledger() const -> const SemanticAssignmentLedger& {
    return m_semantic_ledger;
}

auto DiagnosticSession::execution_timeline() const -> const ExecutionTimeline& {
    return m_timeline;
}

auto DiagnosticSession::chain_manifest() const -> const ChainManifest* {
    return m_manifest.get();
}

auto DiagnosticSession::authoring_verdict() const -> std::optional<AuthoringVerdict> {
    return m_verdict;
}

void DiagnosticSession::set_chain_manifest(std::unique_ptr<ChainManifest> manifest) {
    m_manifest = std::move(manifest);
}

void DiagnosticSession::set_authoring_verdict(AuthoringVerdict verdict) {
    m_verdict = std::move(verdict);
}

auto DiagnosticSession::event_count(Severity severity) const -> uint32_t {
    auto idx = static_cast<size_t>(severity);
    return idx < m_severity_counts.size() ? m_severity_counts[idx] : 0;
}

auto DiagnosticSession::event_count(Category category) const -> uint32_t {
    auto idx = static_cast<size_t>(category);
    return idx < m_category_counts.size() ? m_category_counts[idx] : 0;
}

auto DiagnosticSession::current_frame() const -> uint32_t {
    return m_current_frame;
}

auto DiagnosticSession::events() const -> std::span<const DiagnosticEvent> {
    return m_events;
}

void DiagnosticSession::record_binding(BindingEntry entry) {
    m_binding_ledger.record(std::move(entry));
}

void DiagnosticSession::record_degradation(DegradationEntry entry) {
    m_degradation_ledger.record(std::move(entry));
}

void DiagnosticSession::record_semantic(SemanticEntry entry) {
    m_semantic_ledger.record(std::move(entry));
}

void DiagnosticSession::record_timeline(TimelineEvent event) {
    m_timeline.record(event);
}

void DiagnosticSession::annotate_gpu_duration(TimelineEventType type, uint32_t pass_ordinal,
                                              double gpu_duration_us) {
    m_timeline.annotate_gpu_duration(type, pass_ordinal, gpu_duration_us);
}

void DiagnosticSession::begin_frame(uint32_t frame_index) {
    m_current_frame = frame_index;
    if (!m_has_frame_range) {
        m_identity.frame_start = frame_index;
        m_has_frame_range = true;
    }
    m_identity.frame_end = frame_index;
}

void DiagnosticSession::end_frame() {}

void DiagnosticSession::reset() {
    m_degradation_ledger.clear();
    m_binding_ledger.clear();
    m_semantic_ledger.clear();
    m_timeline.clear();
    m_manifest.reset();
    m_verdict.reset();
    m_severity_counts = {};
    m_category_counts = {};
    m_current_frame = 0;
    m_has_frame_range = false;
    m_identity = {};
    m_events.clear();
}

} // namespace goggles::diagnostics
