#pragma once

#include "diagnostic_event.hpp"
#include "diagnostic_sink.hpp"

#include <vector>

namespace goggles::diagnostics {

/// @brief Collects diagnostic events for test assertions.
class TestHarnessSink : public DiagnosticSink {
public:
    void receive(const DiagnosticEvent& event) override;

    [[nodiscard]] auto events_by_category(Category category) const -> std::vector<DiagnosticEvent>;
    [[nodiscard]] auto events_by_severity(Severity severity) const -> std::vector<DiagnosticEvent>;
    [[nodiscard]] auto event_count() const -> size_t;
    [[nodiscard]] auto all_events() const -> const std::vector<DiagnosticEvent>&;
    void clear();

private:
    std::vector<DiagnosticEvent> m_events;
};

} // namespace goggles::diagnostics
