#include "image_compare.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stb_image.h>
#include <stb_image_write.h>
#include <string>
#include <vector>

namespace goggles::test {

namespace {

auto build_size_mismatch_message(const Image& actual, const Image& reference) -> std::string {
    return "Size mismatch: " + std::to_string(actual.width) + "x" + std::to_string(actual.height) +
           " vs " + std::to_string(reference.width) + "x" + std::to_string(reference.height);
}

auto build_write_failure_message(const std::filesystem::path& path) -> std::string {
    return "Failed to write diff PNG: " + path.string();
}

} // namespace

auto load_png(const std::filesystem::path& path) -> goggles::Result<Image> {
    stbi_set_unpremultiply_on_load(0);

    int width = 0;
    int height = 0;
    int channels = 0;
    constexpr int desired_channels = 4;

    auto* raw_pixels =
        stbi_load(path.string().c_str(), &width, &height, &channels, desired_channels);
    if (raw_pixels == nullptr) {
        const char* reason = stbi_failure_reason();
        const std::string failure_reason = reason == nullptr ? "unknown failure" : reason;
        return make_error<Image>(ErrorCode::file_read_failed,
                                 "Failed to load PNG: " + path.string() + ": " + failure_reason);
    }

    const auto deleter = [](stbi_uc* pixels) { stbi_image_free(pixels); };
    std::unique_ptr<stbi_uc, decltype(deleter)> pixels(raw_pixels, deleter);

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::size_t byte_count = pixel_count * static_cast<std::size_t>(desired_channels);

    Image image;
    image.width = width;
    image.height = height;
    image.channels = desired_channels;
    image.data.assign(pixels.get(), pixels.get() + byte_count);
    return image;
}

auto compare_images(const Image& actual, const Image& reference, const double tolerance,
                    const std::filesystem::path& diff_out) -> CompareResult {
    if (actual.width != reference.width || actual.height != reference.height) {
        CompareResult result;
        result.error_message = build_size_mismatch_message(actual, reference);
        return result;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(actual.width) * static_cast<std::size_t>(actual.height);

    CompareResult result;
    double total_diff_sum = 0.0;
    std::vector<std::uint8_t> diff_data;
    if (!diff_out.empty()) {
        diff_data.resize(pixel_count * 4U, 0);
    }

    for (std::size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
        const std::size_t offset = pixel_index * 4U;
        double pixel_max_diff = 0.0;
        double pixel_channel_sum = 0.0;

        for (std::size_t channel = 0; channel < 4U; ++channel) {
            const int delta = std::abs(static_cast<int>(actual.data[offset + channel]) -
                                       static_cast<int>(reference.data[offset + channel]));
            const double channel_diff = static_cast<double>(delta) / 255.0;
            pixel_max_diff = std::max(pixel_max_diff, channel_diff);
            pixel_channel_sum += channel_diff;
        }

        result.max_channel_diff = std::max(result.max_channel_diff, pixel_max_diff);
        total_diff_sum += pixel_channel_sum / 4.0;

        const bool pixel_failed = pixel_max_diff > tolerance;
        if (pixel_failed) {
            ++result.failing_pixels;
        }

        if (!diff_data.empty()) {
            if (pixel_failed) {
                diff_data[offset + 0U] = 255U;
                diff_data[offset + 1U] = 0U;
                diff_data[offset + 2U] = 0U;
                diff_data[offset + 3U] = 255U;
            } else {
                for (std::size_t channel = 0; channel < 3U; ++channel) {
                    const auto dimmed = static_cast<std::uint8_t>(
                        static_cast<double>(actual.data[offset + channel]) * 0.25);
                    diff_data[offset + channel] = dimmed;
                }
                diff_data[offset + 3U] = 255U;
            }
        }
    }

    if (pixel_count > 0U) {
        result.mean_diff = total_diff_sum / static_cast<double>(pixel_count);
        result.failing_percentage =
            (static_cast<double>(result.failing_pixels) * 100.0) / static_cast<double>(pixel_count);
    }

    if (!diff_out.empty() && result.failing_pixels > 0U) {
        const int stride = actual.width * 4;
        const int write_status = stbi_write_png(diff_out.string().c_str(), actual.width,
                                                actual.height, 4, diff_data.data(), stride);
        if (write_status == 0) {
            result.error_message = build_write_failure_message(diff_out);
        }
    }

    result.passed = result.failing_pixels == 0U && result.error_message.empty();
    return result;
}

} // namespace goggles::test
