#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace goggles::diagnostics {

enum class SemanticClassification : uint8_t { parameter, semantic, static_value, unresolved };

struct SemanticEntry {
    uint32_t pass_ordinal = 0;
    std::string member_name;
    SemanticClassification classification = SemanticClassification::unresolved;
    std::variant<float, std::array<float, 4>> value;
    uint32_t offset = 0;
};

class SemanticAssignmentLedger {
public:
    void record(SemanticEntry entry) { m_entries.push_back(std::move(entry)); }

    [[nodiscard]] auto entries_for_pass(uint32_t pass_ordinal) const -> std::vector<SemanticEntry> {
        std::vector<SemanticEntry> result;
        for (const auto& e : m_entries) {
            if (e.pass_ordinal == pass_ordinal) {
                result.push_back(e);
            }
        }
        return result;
    }

    [[nodiscard]] auto all_entries() const -> const std::vector<SemanticEntry>& {
        return m_entries;
    }

    void clear() { m_entries.clear(); }

private:
    std::vector<SemanticEntry> m_entries;
};

} // namespace goggles::diagnostics
