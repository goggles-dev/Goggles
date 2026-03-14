#pragma once

#include <filesystem>
#include <goggles/filter_chain/error.hpp>
#include <optional>
#include <string>
#include <vector>

namespace goggles::test {

struct ShaderBatchReportOptions {
    std::filesystem::path presets_root;
    std::vector<std::filesystem::path> preset_paths;
    std::optional<std::string> category_filter;
    std::filesystem::path output_path;
    bool strict_mode = false;
};

[[nodiscard]] auto run_shader_batch_report(const ShaderBatchReportOptions& options) -> Result<int>;

} // namespace goggles::test
