#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace goggles::diagnostics {

enum class BindingStatus : uint8_t { resolved, substituted, unresolved };

struct BindingEntry {
    uint32_t pass_ordinal = 0;
    uint32_t binding_slot = 0;
    BindingStatus status = BindingStatus::resolved;
    std::string resource_identity;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint32_t producer_pass_ordinal = UINT32_MAX;
    std::string alias_name;
};

class BindingLedger {
public:
    void record(BindingEntry entry) { m_entries.push_back(std::move(entry)); }

    [[nodiscard]] auto entries_for_pass(uint32_t pass_ordinal) const -> std::vector<BindingEntry> {
        std::vector<BindingEntry> result;
        for (const auto& e : m_entries) {
            if (e.pass_ordinal == pass_ordinal) {
                result.push_back(e);
            }
        }
        return result;
    }

    [[nodiscard]] auto all_entries() const -> const std::vector<BindingEntry>& { return m_entries; }

    void clear() { m_entries.clear(); }

private:
    std::vector<BindingEntry> m_entries;
};

} // namespace goggles::diagnostics
