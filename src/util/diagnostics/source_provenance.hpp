#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace goggles::diagnostics {

struct ProvenanceEntry {
    std::string original_file;
    uint32_t original_line = 0;
    bool rewrite_applied = false;
    std::string rewrite_description;
};

class SourceProvenanceMap {
public:
    void record(uint32_t expanded_line, ProvenanceEntry entry) {
        m_entries[expanded_line] = std::move(entry);
    }

    [[nodiscard]] auto lookup(uint32_t expanded_line) const -> const ProvenanceEntry* {
        auto it = m_entries.find(expanded_line);
        return it != m_entries.end() ? &it->second : nullptr;
    }

    [[nodiscard]] auto size() const -> size_t { return m_entries.size(); }

private:
    std::unordered_map<uint32_t, ProvenanceEntry> m_entries;
};

} // namespace goggles::diagnostics
