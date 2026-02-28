#include "image_compare.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace goggles::test {

// Tolerance for border pixels: must be exactly black
constexpr double BORDER_TOLERANCE = 0.0;
// Tolerance for content pixels: allow minor GPU rounding differences
constexpr double CONTENT_TOLERANCE = 2.0 / 255.0;

// Expected quadrant colors from quadrant_client (RGBA, 0-255)
struct Color {
    uint8_t r, g, b, a;
};
constexpr Color RED = {255, 0, 0, 255};
constexpr Color GREEN = {0, 255, 0, 255};
constexpr Color BLUE = {0, 0, 255, 255};
constexpr Color WHITE = {255, 255, 255, 255};
constexpr Color BLACK = {0, 0, 0, 255};

// Provides deterministic unique paths without heap-allocated state machines
struct TempFile {
    std::filesystem::path path;

    explicit TempFile(std::string_view suffix) {
        static int counter = 0;
        auto tmp = std::filesystem::temp_directory_path();
        // PID + counter ensures no collision across parallel test processes
        path = tmp / (std::string("goggles_ar_test_") + std::to_string(getpid()) + "_" +
                      std::to_string(counter++) + std::string(suffix));
    }
    ~TempFile() { std::filesystem::remove(path); }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
};

// Runs goggles headless and blocks until exit. Returns the child exit status.
auto run_goggles(const std::vector<std::string>& args) -> int {
    std::vector<const char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) {
        argv.push_back(a.c_str());
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        // Child: replace process image with goggles binary
        execvp(argv[0], const_cast<char* const*>(argv.data()));
        // execvp only returns on failure; use _exit to avoid flushing parent stdio
        _exit(127);
    }

    int status = -1;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Returns normalised [0,1] channel values for the pixel at (x,y)
struct PixelF {
    double r, g, b, a;
};

auto sample_pixel(const Image& img, int x, int y) -> PixelF {
    const int idx = (y * img.width + x) * img.channels;
    const auto* d = img.data.data();
    return {
        d[idx + 0] / 255.0,
        d[idx + 1] / 255.0,
        d[idx + 2] / 255.0,
        img.channels == 4 ? d[idx + 3] / 255.0 : 1.0,
    };
}

// Checks a single pixel against an expected color within per-channel tolerance
void check_pixel(const Image& img, int x, int y, Color expected, double tolerance,
                 std::string_view label) {
    const auto px = sample_pixel(img, x, y);
    const double er = expected.r / 255.0;
    const double eg = expected.g / 255.0;
    const double eb = expected.b / 255.0;
    INFO(label << " pixel (" << x << "," << y << ") expected rgb(" << (int)expected.r << ","
               << (int)expected.g << "," << (int)expected.b << ") got rgb(" << px.r * 255 << ","
               << px.g * 255 << "," << px.b * 255 << ")");
    CHECK(std::abs(px.r - er) <= tolerance);
    CHECK(std::abs(px.g - eg) <= tolerance);
    CHECK(std::abs(px.b - eb) <= tolerance);
}

// Samples the centre of each quadrant within the content rectangle and checks
// all four expected colors.
void check_quadrants(const Image& img, int cx, int cy, int cw, int ch) {
    // quadrant_client paints the fixed 640x480 source into four equal quadrants;
    // the compositor scales that into the content rectangle [cx,cy,cw,ch].
    const int qx0 = cx + cw / 4;
    const int qy0 = cy + ch / 4;
    const int qx1 = cx + 3 * cw / 4;
    const int qy1 = cy + 3 * ch / 4;

    check_pixel(img, qx0, qy0, RED, CONTENT_TOLERANCE, "top-left");
    check_pixel(img, qx1, qy0, GREEN, CONTENT_TOLERANCE, "top-right");
    check_pixel(img, qx0, qy1, BLUE, CONTENT_TOLERANCE, "bottom-left");
    check_pixel(img, qx1, qy1, WHITE, CONTENT_TOLERANCE, "bottom-right");
}

// Validates that pixels at specific horizontal or vertical edge positions are black,
// confirming letterbox/pillarbox bars are present and correctly placed.
void check_border_column(const Image& img, int x, std::string_view label) {
    // Sample top, middle, and bottom of the column
    check_pixel(img, x, img.height / 6, BLACK, BORDER_TOLERANCE, label);
    check_pixel(img, x, img.height / 2, BLACK, BORDER_TOLERANCE, label);
    check_pixel(img, x, 5 * img.height / 6, BLACK, BORDER_TOLERANCE, label);
}

void check_border_row(const Image& img, int y, std::string_view label) {
    check_pixel(img, img.width / 6, y, BLACK, BORDER_TOLERANCE, label);
    check_pixel(img, img.width / 2, y, BLACK, BORDER_TOLERANCE, label);
    check_pixel(img, 5 * img.width / 6, y, BLACK, BORDER_TOLERANCE, label);
}

// Builds the goggles argument list for a headless capture run
auto build_args(const std::string& output_path, const std::string& config_path,
                uint32_t app_width = 0, uint32_t app_height = 0) -> std::vector<std::string> {
    std::vector<std::string> args{
        GOGGLES_BINARY, "--headless", "--frames", "5",
        "--output",     output_path,  "--config", config_path,
    };

    if (app_width > 0 && app_height > 0) {
        args.emplace_back("--app-width");
        args.emplace_back(std::to_string(app_width));
        args.emplace_back("--app-height");
        args.emplace_back(std::to_string(app_height));
    }

    args.emplace_back("--");
    args.emplace_back(QUADRANT_CLIENT_BINARY);
    return args;
}

// ── Test cases ────────────────────────────────────────────────────────────────
//
// Geometry derivations (source always 640×480):
//   fit_letterbox  1920×1080: src_ar(4:3) < vp_ar(16:9) → fill height
//                             content 1440×1080, offset_x=240, offset_y=0
//                             side bars: x<240, x>=1680
//   fit_pillarbox   800×600:  force headless output to 800×600 via
//                             --app-width/--app-height; src_ar(4:3) = vp_ar(4:3)
//                             → perfect fit, content 800×600, no bars
//   fill           1920×1080: src_ar(4:3) < vp_ar(16:9) → fill width
//                             content 1920×1440, offset_y negative → image overflows
//                             entire output filled, no black bars visible
//   stretch        1920×1080: fills all 1920×1080, no bars
//   integer_1x     1920×1080: scale=1 → content 640×480, offset_x=640, offset_y=300
//                             bars everywhere outside that rectangle
//   integer_2x     1920×1080: scale=2 → content 1280×960, offset_x=320, offset_y=60
//   integer_auto   1920×1080: auto scale = min(3,2) = 2 → same as 2x
//   dynamic        1920×1080: behaves like fit → same bars as fit_letterbox

TEST_CASE("aspect ratio - fit letterbox (1920x1080)", "[visual][aspect]") {
    const TempFile tmp(".png");
    const std::string config = std::string(VISUAL_CONFIGS_DIR) + "/aspect_fit_letterbox.toml";

    const auto exit_code = run_goggles(build_args(tmp.path.string(), config));
    REQUIRE(exit_code == 0);

    const auto img_result = load_png(tmp.path);
    REQUIRE(img_result.has_value());
    const auto& img = *img_result;
    REQUIRE(img.width == 1920);
    REQUIRE(img.height == 1080);

    // Side pillar bars at x < 240 and x >= 1680 (letterboxed left/right)
    check_border_column(img, 100, "left bar");
    check_border_column(img, 1750, "right bar");

    // Content spans x=[240,1680), y=[0,1080)
    constexpr int CX = 240, CY = 0, CW = 1440, CH = 1080;
    check_quadrants(img, CX, CY, CW, CH);
}

TEST_CASE("aspect ratio - fit perfect (800x600)", "[visual][aspect]") {
    const TempFile tmp(".png");
    const std::string config = std::string(VISUAL_CONFIGS_DIR) + "/aspect_fit_pillarbox.toml";

    // Force headless output to 800x600 (4:3) while source remains quadrant_client
    // content at 4:3. fit mode should produce a perfect fill with no bars.
    const auto exit_code = run_goggles(build_args(tmp.path.string(), config, 800, 600));
    REQUIRE(exit_code == 0);

    const auto img_result = load_png(tmp.path);
    REQUIRE(img_result.has_value());
    const auto& img = *img_result;
    REQUIRE(img.width == 800);
    REQUIRE(img.height == 600);

    // Perfect fit: content spans the full output, no black bars.
    check_quadrants(img, 0, 0, 800, 600);
}

TEST_CASE("aspect ratio - fill (1920x1080)", "[visual][aspect]") {
    const TempFile tmp(".png");
    const std::string config = std::string(VISUAL_CONFIGS_DIR) + "/aspect_fill.toml";

    const auto exit_code = run_goggles(build_args(tmp.path.string(), config));
    REQUIRE(exit_code == 0);

    const auto img_result = load_png(tmp.path);
    REQUIRE(img_result.has_value());
    const auto& img = *img_result;
    REQUIRE(img.width == 1920);
    REQUIRE(img.height == 1080);

    // Content overflows the viewport (offset_y negative), so the output is
    // entirely covered — no black borders should appear at center.
    // We verify that the image center is non-black content.
    const auto center = sample_pixel(img, 960, 540);
    const bool center_is_black = center.r < CONTENT_TOLERANCE && center.g < CONTENT_TOLERANCE &&
                                 center.b < CONTENT_TOLERANCE;
    CHECK_FALSE(center_is_black);
}

TEST_CASE("aspect ratio - stretch (1920x1080)", "[visual][aspect]") {
    const TempFile tmp(".png");
    const std::string config = std::string(VISUAL_CONFIGS_DIR) + "/aspect_stretch.toml";

    const auto exit_code = run_goggles(build_args(tmp.path.string(), config));
    REQUIRE(exit_code == 0);

    const auto img_result = load_png(tmp.path);
    REQUIRE(img_result.has_value());
    const auto& img = *img_result;
    REQUIRE(img.width == 1920);
    REQUIRE(img.height == 1080);

    // Stretching fills the entire viewport; no bars
    check_quadrants(img, 0, 0, 1920, 1080);
}

TEST_CASE("aspect ratio - integer scale 1x (1920x1080)", "[visual][aspect]") {
    const TempFile tmp(".png");
    const std::string config = std::string(VISUAL_CONFIGS_DIR) + "/aspect_integer_1x.toml";

    const auto exit_code = run_goggles(build_args(tmp.path.string(), config));
    REQUIRE(exit_code == 0);

    const auto img_result = load_png(tmp.path);
    REQUIRE(img_result.has_value());
    const auto& img = *img_result;
    REQUIRE(img.width == 1920);
    REQUIRE(img.height == 1080);

    // Content 640×480 centred: offset_x=640, offset_y=300
    // Bars present in all four surrounding regions
    check_border_column(img, 300, "left bar");
    check_border_column(img, 1400, "right bar");
    check_border_row(img, 100, "top bar");
    check_border_row(img, 900, "bottom bar");

    constexpr int CX = 640, CY = 300, CW = 640, CH = 480;
    check_quadrants(img, CX, CY, CW, CH);
}

TEST_CASE("aspect ratio - integer scale 2x (1920x1080)", "[visual][aspect]") {
    const TempFile tmp(".png");
    const std::string config = std::string(VISUAL_CONFIGS_DIR) + "/aspect_integer_2x.toml";

    const auto exit_code = run_goggles(build_args(tmp.path.string(), config));
    REQUIRE(exit_code == 0);

    const auto img_result = load_png(tmp.path);
    REQUIRE(img_result.has_value());
    const auto& img = *img_result;
    REQUIRE(img.width == 1920);
    REQUIRE(img.height == 1080);

    // Content 1280×960 centred: offset_x=320, offset_y=60
    check_border_column(img, 100, "left bar");
    check_border_column(img, 1700, "right bar");
    check_border_row(img, 30, "top bar");
    check_border_row(img, 1050, "bottom bar");

    constexpr int CX = 320, CY = 60, CW = 1280, CH = 960;
    check_quadrants(img, CX, CY, CW, CH);
}

TEST_CASE("aspect ratio - integer scale auto (1920x1080)", "[visual][aspect]") {
    const TempFile tmp(".png");
    const std::string config = std::string(VISUAL_CONFIGS_DIR) + "/aspect_integer_auto.toml";

    const auto exit_code = run_goggles(build_args(tmp.path.string(), config));
    REQUIRE(exit_code == 0);

    const auto img_result = load_png(tmp.path);
    REQUIRE(img_result.has_value());
    const auto& img = *img_result;
    REQUIRE(img.width == 1920);
    REQUIRE(img.height == 1080);

    // auto scale = min(1920/640, 1080/480) = min(3,2) = 2 → identical geometry to 2x
    check_border_column(img, 100, "left bar");
    check_border_column(img, 1700, "right bar");
    check_border_row(img, 30, "top bar");
    check_border_row(img, 1050, "bottom bar");

    constexpr int CX = 320, CY = 60, CW = 1280, CH = 960;
    check_quadrants(img, CX, CY, CW, CH);
}

TEST_CASE("aspect ratio - dynamic (1920x1080)", "[visual][aspect]") {
    const TempFile tmp(".png");
    const std::string config = std::string(VISUAL_CONFIGS_DIR) + "/aspect_dynamic.toml";

    const auto exit_code = run_goggles(build_args(tmp.path.string(), config));
    REQUIRE(exit_code == 0);

    const auto img_result = load_png(tmp.path);
    REQUIRE(img_result.has_value());
    const auto& img = *img_result;
    REQUIRE(img.width == 1920);
    REQUIRE(img.height == 1080);

    // Dynamic falls back to fit behaviour while the source resolution is stable,
    // producing the same geometry as fit_letterbox: side bars x<240 and x>=1680
    check_border_column(img, 100, "left bar");
    check_border_column(img, 1750, "right bar");

    constexpr int CX = 240, CY = 0, CW = 1440, CH = 1080;
    check_quadrants(img, CX, CY, CW, CH);
}

} // namespace goggles::test
