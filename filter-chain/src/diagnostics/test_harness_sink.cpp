#include "test_harness_sink.hpp"

namespace goggles::diagnostics {

void TestHarnessSink::receive(const DiagnosticEvent& event) {
    m_events.push_back(event);
}

auto TestHarnessSink::events_by_category(Category category) const -> std::vector<DiagnosticEvent> {
    std::vector<DiagnosticEvent> result;
    for (const auto& e : m_events) {
        if (e.category == category) {
            result.push_back(e);
        }
    }
    return result;
}

auto TestHarnessSink::events_by_severity(Severity severity) const -> std::vector<DiagnosticEvent> {
    std::vector<DiagnosticEvent> result;
    for (const auto& e : m_events) {
        if (e.severity == severity) {
            result.push_back(e);
        }
    }
    return result;
}

auto TestHarnessSink::event_count() const -> size_t {
    return m_events.size();
}

auto TestHarnessSink::all_events() const -> const std::vector<DiagnosticEvent>& {
    return m_events;
}

void TestHarnessSink::clear() {
    m_events.clear();
}

} // namespace goggles::diagnostics
