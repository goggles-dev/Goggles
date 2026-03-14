#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace goggles::diagnostics {

enum class CompileStage : uint8_t { vertex, fragment };

struct StageReport {
    CompileStage stage = CompileStage::vertex;
    bool success = false;
    std::vector<std::string> messages;
    double timing_us = 0.0;
    bool cache_hit = false;
};

class CompileReport {
public:
    void add_stage(StageReport report) { m_stages.push_back(std::move(report)); }

    [[nodiscard]] auto stages() const -> const std::vector<StageReport>& { return m_stages; }

    [[nodiscard]] auto all_succeeded() const -> bool {
        for (const auto& s : m_stages) {
            if (!s.success) {
                return false;
            }
        }
        return !m_stages.empty();
    }

    [[nodiscard]] auto total_timing_us() const -> double {
        double total = 0.0;
        for (const auto& s : m_stages) {
            total += s.timing_us;
        }
        return total;
    }

private:
    std::vector<StageReport> m_stages;
};

} // namespace goggles::diagnostics
