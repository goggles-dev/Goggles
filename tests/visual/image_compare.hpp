#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <util/error.hpp>
#include <vector>

namespace goggles::test {

struct Image {
    std::vector<std::uint8_t> data;
    int width = 0;
    int height = 0;
    int channels = 0;
};

struct CompareResult {
    bool passed = false;
    double max_channel_diff = 0.0;
    double mean_diff = 0.0;
    std::uint32_t failing_pixels = 0;
    double failing_percentage = 0.0;
    std::string error_message;
};

[[nodiscard]] auto load_png(const std::filesystem::path& path) -> goggles::Result<Image>;

[[nodiscard]] auto compare_images(const Image& actual, const Image& reference, double tolerance,
                                  const std::filesystem::path& diff_out = {}) -> CompareResult;

} // namespace goggles::test
