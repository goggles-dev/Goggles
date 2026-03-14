#include "shader_batch_report.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

auto read_text(const std::filesystem::path& path) -> std::string {
    std::ifstream in(path);
    return {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("Shader batch report writes diagnostic JSON", "[render][shader_batch]") {
    const auto valid_preset = std::filesystem::path(FILTER_CHAIN_ASSET_DIR) /
                              "diagnostics/authoring_corpus/"
                              "valid/pass_through.slangp";
    REQUIRE(std::filesystem::exists(valid_preset));

    const auto temp_dir = std::filesystem::temp_directory_path() / "goggles_shader_batch_test";
    std::filesystem::create_directories(temp_dir);
    const auto invalid_preset = temp_dir / "invalid_missing_shader.slangp";
    {
        std::ofstream out(invalid_preset);
        out << "shaders = \"1\"\n";
        out << "shader0 = \"missing_shader.slang\"\n";
    }

    const auto output_path = temp_dir / "shader_test_results.json";
    auto result = goggles::test::run_shader_batch_report({
        .presets_root = {},
        .preset_paths = {valid_preset, invalid_preset},
        .category_filter = std::nullopt,
        .output_path = output_path,
    });

    REQUIRE(result);
    CHECK(*result == 1);
    REQUIRE(std::filesystem::exists(output_path));

    std::ifstream in(output_path);
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(json.find("\"authoring_verdict\"") != std::string::npos);
    CHECK(json.find("\"compile_report\"") != std::string::npos);
    CHECK(json.find("\"reflection_summary\"") != std::string::npos);
    CHECK(json.find("\"conformance_findings\"") != std::string::npos);
    CHECK(json.find("\"failed\": 1") != std::string::npos);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Shader batch report validates maintained authoring corpus categories",
          "[render][shader_batch]") {
    const auto corpus_root =
        std::filesystem::path(FILTER_CHAIN_ASSET_DIR) / "diagnostics/authoring_corpus";
    REQUIRE(std::filesystem::exists(corpus_root / "valid/pass_through.slangp"));
    REQUIRE(std::filesystem::exists(corpus_root / "invalid_parse/malformed.slangp"));
    REQUIRE(std::filesystem::exists(corpus_root / "invalid_compile/broken_shader.slangp"));
    REQUIRE(std::filesystem::exists(corpus_root / "reflection_loss/reflectionless.slangp"));

    const auto temp_dir = std::filesystem::temp_directory_path() / "goggles_shader_batch_corpus";
    std::filesystem::create_directories(temp_dir);
    const auto output_path = temp_dir / "shader_test_results.json";

    auto result = goggles::test::run_shader_batch_report({
        .presets_root = corpus_root,
        .preset_paths = {},
        .category_filter = std::nullopt,
        .output_path = output_path,
        .strict_mode = true,
    });

    REQUIRE(result);
    CHECK(*result == 1);
    const auto json = read_text(output_path);
    CHECK(json.find("\"path\": \"" + (corpus_root / "valid/pass_through.slangp").generic_string() +
                    "\"") != std::string::npos);
    CHECK(json.find("\"path\": \"" +
                    (corpus_root / "invalid_parse/malformed.slangp").generic_string() + "\",") !=
          std::string::npos);
    CHECK(json.find("\"path\": \"" +
                    (corpus_root / "invalid_compile/broken_shader.slangp").generic_string() +
                    "\"") != std::string::npos);
    CHECK(json.find("\"path\": \"" +
                    (corpus_root / "reflection_loss/reflectionless.slangp").generic_string() +
                    "\"") != std::string::npos);
    CHECK(json.find("\"authoring_verdict\": \"pass\"") != std::string::npos);
    CHECK(json.find("\"authoring_verdict\": \"fail\"") != std::string::npos);
    CHECK(json.find("reflection_loss:pass=0") != std::string::npos);
    CHECK(json.find("Strict mode rejected passes with reflection loss") != std::string::npos);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Shader batch report preserves source-mapped compile diagnostics",
          "[render][shader_batch]") {
    const auto temp_dir = std::filesystem::temp_directory_path() / "goggles_shader_batch_compile";
    std::filesystem::create_directories(temp_dir);

    const auto invalid_shader = temp_dir / "broken_shader.slang";
    {
        std::ofstream out(invalid_shader);
        out << "#pragma stage vertex\n";
        out << "void main() {\n";
        out << "    this is not valid glsl\n";
        out << "}\n";
        out << "#pragma stage fragment\n";
        out << "layout(location = 0) out vec4 FragColor;\n";
        out << "void main() { FragColor = vec4(1.0); }\n";
    }

    const auto invalid_preset = temp_dir / "invalid_compile.slangp";
    {
        std::ofstream out(invalid_preset);
        out << "shaders = \"1\"\n";
        out << "shader0 = \"broken_shader.slang\"\n";
    }

    const auto output_path = temp_dir / "shader_test_results.json";
    auto result = goggles::test::run_shader_batch_report({
        .presets_root = {},
        .preset_paths = {invalid_preset},
        .category_filter = std::nullopt,
        .output_path = output_path,
    });

    REQUIRE(result);
    CHECK(*result == 1);
    REQUIRE(std::filesystem::exists(output_path));

    std::ifstream in(output_path);
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(json.find("\"authoring_verdict\": \"fail\"") != std::string::npos);
    CHECK(json.find("\"compile_report\"") != std::string::npos);
    CHECK(json.find(invalid_shader.string()) != std::string::npos);
    CHECK(json.find(":3") != std::string::npos);

    std::filesystem::remove_all(temp_dir);
}
