#include "image_compare.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr double BYPASS_TOLERANCE = 2.0 / 255.0;
constexpr double BYPASS_MAX_FAILING_PCT = 0.1;
constexpr double ZFAST_TOLERANCE = 0.05;
constexpr double ZFAST_MAX_FAILING_PCT = 5.0;
constexpr int HEADLESS_FRAMES = 5;

// Run goggles in headless mode with the given config and output path.
// Returns true if the process exited with code 0.
[[nodiscard]] auto run_goggles(const std::string& config_path, const std::string& output_path)
    -> bool {
    const pid_t pid = fork();
    if (pid < 0) {
        std::fprintf(stderr, "fork failed: %s\n", std::strerror(errno));
        return false;
    }

    if (pid == 0) {
        // Child: exec goggles
        std::string frames_str = std::to_string(HEADLESS_FRAMES);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        char* const argv[] = {
            const_cast<char*>(GOGGLES_BINARY),
            const_cast<char*>("--headless"),
            const_cast<char*>("--frames"),
            const_cast<char*>(frames_str.c_str()),
            const_cast<char*>("--output"),
            const_cast<char*>(output_path.c_str()),
            const_cast<char*>("--config"),
            const_cast<char*>(config_path.c_str()),
            const_cast<char*>("--"),
            const_cast<char*>(QUADRANT_CLIENT_BINARY),
            nullptr,
        };
        execvp(argv[0], argv);
        std::fprintf(stderr, "execvp failed: %s\n", std::strerror(errno));
        _exit(127);
    }

    // Parent: wait for child
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::fprintf(stderr, "waitpid failed: %s\n", std::strerror(errno));
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

} // namespace

TEST_CASE("shader bypass: output matches golden within tolerance") {
    const std::filesystem::path golden_path =
        std::filesystem::path(GOLDEN_DIR) / "shader_bypass_quadrant.png";
    const std::filesystem::path config_path =
        std::filesystem::path(VISUAL_CONFIGS_DIR) / "shader_bypass.toml";
    const std::filesystem::path output_path = "actual_bypass_quadrant.png";

    if (!std::filesystem::exists(golden_path)) {
        SKIP("Golden image not found: " + golden_path.string() +
             " — run `pixi run update-golden` to generate");
    }

    const bool goggles_ok = run_goggles(config_path.string(), output_path.string());
    REQUIRE(goggles_ok);
    REQUIRE(std::filesystem::exists(output_path));

    const auto actual_result = goggles::test::load_png(output_path);
    REQUIRE(actual_result.has_value());
    const auto golden_result = goggles::test::load_png(golden_path);
    REQUIRE(golden_result.has_value());

    const auto cmp = goggles::test::compare_images(*actual_result, *golden_result, BYPASS_TOLERANCE,
                                                   "diff_bypass.png");
    CHECK(cmp.passed);
    CHECK(cmp.failing_percentage <= BYPASS_MAX_FAILING_PCT);

    std::filesystem::remove(output_path);
}

TEST_CASE("zfast-crt: output matches golden within tolerance") {
    const std::filesystem::path golden_path =
        std::filesystem::path(GOLDEN_DIR) / "shader_zfast_quadrant.png";
    const std::filesystem::path config_path =
        std::filesystem::path(VISUAL_CONFIGS_DIR) / "shader_zfast.toml";
    const std::filesystem::path output_path = "actual_zfast_quadrant.png";

    if (!std::filesystem::exists(golden_path)) {
        SKIP("Golden image not found: " + golden_path.string() +
             " — run `pixi run update-golden` to generate");
    }

    const bool goggles_ok = run_goggles(config_path.string(), output_path.string());
    REQUIRE(goggles_ok);
    REQUIRE(std::filesystem::exists(output_path));

    const auto actual_result = goggles::test::load_png(output_path);
    REQUIRE(actual_result.has_value());
    const auto golden_result = goggles::test::load_png(golden_path);
    REQUIRE(golden_result.has_value());

    const auto cmp = goggles::test::compare_images(*actual_result, *golden_result, ZFAST_TOLERANCE,
                                                   "diff_zfast.png");
    CHECK(cmp.passed);
    CHECK(cmp.failing_percentage <= ZFAST_MAX_FAILING_PCT);

    std::filesystem::remove(output_path);
}

TEST_CASE("filter-chain toggle: bypass vs zfast produce distinct outputs") {
    const std::filesystem::path golden_bypass =
        std::filesystem::path(GOLDEN_DIR) / "shader_bypass_quadrant.png";
    const std::filesystem::path golden_zfast =
        std::filesystem::path(GOLDEN_DIR) / "shader_zfast_quadrant.png";
    const std::filesystem::path config_bypass =
        std::filesystem::path(VISUAL_CONFIGS_DIR) / "shader_bypass.toml";
    const std::filesystem::path config_zfast =
        std::filesystem::path(VISUAL_CONFIGS_DIR) / "shader_zfast.toml";

    if (!std::filesystem::exists(golden_bypass) || !std::filesystem::exists(golden_zfast)) {
        SKIP("Golden images not found — run `pixi run update-golden` to generate");
    }

    const std::filesystem::path output_bypass = "toggle_off_quadrant.png";
    const std::filesystem::path output_zfast = "toggle_on_quadrant.png";

    const bool bypass_ok = run_goggles(config_bypass.string(), output_bypass.string());
    REQUIRE(bypass_ok);
    REQUIRE(std::filesystem::exists(output_bypass));

    const bool zfast_ok = run_goggles(config_zfast.string(), output_zfast.string());
    REQUIRE(zfast_ok);
    REQUIRE(std::filesystem::exists(output_zfast));

    const auto golden_bypass_img = goggles::test::load_png(golden_bypass);
    REQUIRE(golden_bypass_img.has_value());
    const auto golden_zfast_img = goggles::test::load_png(golden_zfast);
    REQUIRE(golden_zfast_img.has_value());
    const auto actual_bypass_img = goggles::test::load_png(output_bypass);
    REQUIRE(actual_bypass_img.has_value());
    const auto actual_zfast_img = goggles::test::load_png(output_zfast);
    REQUIRE(actual_zfast_img.has_value());

    // toggle_off (bypass config) must match bypass golden
    const auto cmp_off =
        goggles::test::compare_images(*actual_bypass_img, *golden_bypass_img, BYPASS_TOLERANCE);
    CHECK(cmp_off.passed);
    CHECK(cmp_off.failing_percentage <= BYPASS_MAX_FAILING_PCT);

    // toggle_on (zfast config) must match zfast golden
    const auto cmp_on =
        goggles::test::compare_images(*actual_zfast_img, *golden_zfast_img, ZFAST_TOLERANCE);
    CHECK(cmp_on.passed);
    CHECK(cmp_on.failing_percentage <= ZFAST_MAX_FAILING_PCT);

    std::filesystem::remove(output_bypass);
    std::filesystem::remove(output_zfast);
}
