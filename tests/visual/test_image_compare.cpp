#include "image_compare.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace {

auto make_image(const int width, const int height, const std::uint8_t r, const std::uint8_t g,
                const std::uint8_t b, const std::uint8_t a) -> goggles::test::Image {
    goggles::test::Image image;
    image.width = width;
    image.height = height;
    image.channels = 4;
    image.data.resize(static_cast<std::size_t>(width * height * 4), 0U);
    for (int pixel_index = 0; pixel_index < width * height; ++pixel_index) {
        const std::size_t offset = static_cast<std::size_t>(pixel_index * 4);
        image.data[offset + 0U] = r;
        image.data[offset + 1U] = g;
        image.data[offset + 2U] = b;
        image.data[offset + 3U] = a;
    }
    return image;
}

} // namespace

TEST_CASE("identical images pass") {
    const auto actual = make_image(4, 4, 12U, 34U, 56U, 255U);
    const auto reference = make_image(4, 4, 12U, 34U, 56U, 255U);

    const auto result = goggles::test::compare_images(actual, reference, 0.0);
    CHECK(result.passed);
    CHECK(result.max_channel_diff == 0.0);
    CHECK(result.failing_pixels == 0U);
}

TEST_CASE("1-value diff fails at tolerance 0") {
    auto actual = make_image(4, 4, 0U, 0U, 0U, 255U);
    auto reference = make_image(4, 4, 0U, 0U, 0U, 255U);
    reference.data[0] = static_cast<std::uint8_t>(reference.data[0] + 1U);

    const auto result = goggles::test::compare_images(actual, reference, 0.0);
    CHECK_FALSE(result.passed);
    CHECK(result.failing_pixels >= 1U);
}

TEST_CASE("tolerance allows small diffs") {
    const auto actual = make_image(4, 4, 32U, 64U, 96U, 128U);
    const auto reference = make_image(4, 4, 34U, 66U, 98U, 130U);

    const auto result = goggles::test::compare_images(actual, reference, 2.0 / 255.0);
    CHECK(result.passed);
}

TEST_CASE("size mismatch fails") {
    const auto actual = make_image(4, 4, 1U, 2U, 3U, 255U);
    const auto reference = make_image(8, 8, 1U, 2U, 3U, 255U);

    const auto result = goggles::test::compare_images(actual, reference, 0.0);
    CHECK_FALSE(result.passed);
    CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("diff image written on failure") {
    const auto actual = make_image(4, 4, 0U, 0U, 0U, 255U);
    const auto reference = make_image(4, 4, 255U, 255U, 255U, 255U);
    const std::filesystem::path diff_path = "test_diff_output.png";

    std::filesystem::remove(diff_path);
    const auto result = goggles::test::compare_images(actual, reference, 0.0, diff_path);

    CHECK_FALSE(result.passed);
    CHECK(std::filesystem::exists(diff_path));

    std::filesystem::remove(diff_path);
}
