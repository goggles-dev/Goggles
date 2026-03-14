#include "shader_batch_report.hpp"

#include <iostream>
#include <string>

namespace {

auto parse_args(int argc, char** argv) -> goggles::test::ShaderBatchReportOptions {
    goggles::test::ShaderBatchReportOptions options;
    options.presets_root = std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "shaders";
    options.output_path = std::filesystem::temp_directory_path() / "shader_test_results.json";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--category" && i + 1 < argc) {
            options.category_filter = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            options.output_path = argv[++i];
        } else if (arg == "--root" && i + 1 < argc) {
            options.presets_root = argv[++i];
        }
    }

    return options;
}

} // namespace

int main(int argc, char** argv) {
    const auto options = parse_args(argc, argv);
    const auto result = goggles::test::run_shader_batch_report(options);
    if (!result) {
        std::cerr << result.error().message << '\n';
        return 2;
    }
    return *result;
}
