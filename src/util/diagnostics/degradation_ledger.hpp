#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace goggles::diagnostics {

enum class DegradationType : uint8_t {
    texture_fallback,
    semantic_unresolved,
    reflection_loss,
};

struct DegradationEntry {
    uint32_t pass_ordinal = 0;
    std::string expected_resource;
    std::string substituted_resource;
    uint32_t frame_index = 0;
    DegradationType type = DegradationType::texture_fallback;
};

class DegradationLedger {
public:
    void record(DegradationEntry entry) { m_entries.push_back(std::move(entry)); }

    [[nodiscard]] auto entries_for_pass(uint32_t pass_ordinal) const
        -> std::vector<DegradationEntry> {
        std::vector<DegradationEntry> result;
        for (const auto& e : m_entries) {
            if (e.pass_ordinal == pass_ordinal) {
                result.push_back(e);
            }
        }
        return result;
    }

    [[nodiscard]] auto all_entries() const -> const std::vector<DegradationEntry>& {
        return m_entries;
    }

    void clear() { m_entries.clear(); }

private:
    std::vector<DegradationEntry> m_entries;
};

} // namespace goggles::diagnostics
