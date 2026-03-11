#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace goggles::diagnostics {

struct ManifestPassEntry {
    uint32_t ordinal = 0;
    std::string shader_path;
    std::string scale_type_x;
    std::string scale_type_y;
    float scale_x = 1.0F;
    float scale_y = 1.0F;
    std::string format_override;
    std::string wrap_mode;
    std::string alias;
};

struct ManifestTextureEntry {
    std::string name;
    std::string path;
    std::string filter_mode;
    bool mipmap = false;
    std::string wrap_mode;
};

struct TemporalRequirements {
    uint32_t history_depth = 0;
    std::vector<uint32_t> feedback_producer_passes;
    std::vector<uint32_t> feedback_consumer_passes;
};

class ChainManifest {
public:
    [[nodiscard]] auto passes() const -> const std::vector<ManifestPassEntry>& { return m_passes; }
    [[nodiscard]] auto textures() const -> const std::vector<ManifestTextureEntry>& {
        return m_textures;
    }
    [[nodiscard]] auto aliases() const -> const std::vector<std::string>& { return m_aliases; }
    [[nodiscard]] auto temporal() const -> const TemporalRequirements& { return m_temporal; }

    void add_pass(ManifestPassEntry entry) { m_passes.push_back(std::move(entry)); }
    void add_texture(ManifestTextureEntry entry) { m_textures.push_back(std::move(entry)); }
    void add_alias(std::string alias) { m_aliases.push_back(std::move(alias)); }
    void set_temporal(TemporalRequirements temporal) { m_temporal = std::move(temporal); }

private:
    std::vector<ManifestPassEntry> m_passes;
    std::vector<ManifestTextureEntry> m_textures;
    std::vector<std::string> m_aliases;
    TemporalRequirements m_temporal;
};

} // namespace goggles::diagnostics
