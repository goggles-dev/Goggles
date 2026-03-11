#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace goggles::diagnostics {

enum class TimelineEventType : uint8_t {
    prechain_start,
    prechain_end,
    pass_start,
    pass_end,
    final_composition_start,
    final_composition_end,
    history_push,
    feedback_copy,
    allocation,
};

struct TimelineEvent {
    TimelineEventType type = TimelineEventType::pass_start;
    uint32_t pass_ordinal = 0;
    uint64_t cpu_timestamp_ns = 0;
    std::optional<double> gpu_duration_us;
};

class ExecutionTimeline {
public:
    void record(TimelineEvent event) { m_events.push_back(event); }

    void annotate_gpu_duration(TimelineEventType type, uint32_t pass_ordinal,
                               double gpu_duration_us) {
        const auto event_it = std::find_if(
            m_events.rbegin(), m_events.rend(), [type, pass_ordinal](const auto& event) {
                return event.pass_ordinal == pass_ordinal && event.type == type;
            });
        if (event_it != m_events.rend()) {
            event_it->gpu_duration_us = gpu_duration_us;
        }
    }

    [[nodiscard]] auto events() const -> const std::vector<TimelineEvent>& { return m_events; }

    [[nodiscard]] auto events_for_pass(uint32_t pass_ordinal) const -> std::vector<TimelineEvent> {
        std::vector<TimelineEvent> result;
        for (const auto& e : m_events) {
            if (e.pass_ordinal == pass_ordinal) {
                result.push_back(e);
            }
        }
        return result;
    }

    void clear() { m_events.clear(); }

private:
    std::vector<TimelineEvent> m_events;
};

} // namespace goggles::diagnostics
