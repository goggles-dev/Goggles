#include "diagnostics/source_provenance.hpp"
#include "shader/retroarch_preprocessor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace goggles::diagnostics;

TEST_CASE("SourceProvenanceMap empty by default", "[diagnostics][provenance]") {
    SourceProvenanceMap map;
    REQUIRE(map.size() == 0);
    REQUIRE(map.lookup(1) == nullptr);
}

TEST_CASE("SourceProvenanceMap records and retrieves entries", "[diagnostics][provenance]") {
    SourceProvenanceMap map;
    map.record(1, {.original_file = "foo.glsl",
                   .original_line = 10,
                   .rewrite_applied = false,
                   .rewrite_description = {}});
    map.record(2, {.original_file = "foo.glsl",
                   .original_line = 11,
                   .rewrite_applied = false,
                   .rewrite_description = {}});
    map.record(5, {.original_file = "bar.glsl",
                   .original_line = 1,
                   .rewrite_applied = false,
                   .rewrite_description = {}});

    REQUIRE(map.size() == 3);

    auto* entry1 = map.lookup(1);
    REQUIRE(entry1 != nullptr);
    REQUIRE(entry1->original_file == "foo.glsl");
    REQUIRE(entry1->original_line == 10);
    REQUIRE(entry1->rewrite_applied == false);

    auto* entry5 = map.lookup(5);
    REQUIRE(entry5 != nullptr);
    REQUIRE(entry5->original_file == "bar.glsl");
    REQUIRE(entry5->original_line == 1);

    REQUIRE(map.lookup(3) == nullptr);
}

TEST_CASE("SourceProvenanceMap tracks rewrite flag", "[diagnostics][provenance]") {
    SourceProvenanceMap map;
    map.record(10, {.original_file = "shader.glsl",
                    .original_line = 42,
                    .rewrite_applied = true,
                    .rewrite_description = "Slang compatibility fix"});

    auto* entry = map.lookup(10);
    REQUIRE(entry != nullptr);
    REQUIRE(entry->rewrite_applied == true);
    REQUIRE(entry->rewrite_description == "Slang compatibility fix");
}

TEST_CASE("SourceProvenanceMap overwrites on duplicate key", "[diagnostics][provenance]") {
    SourceProvenanceMap map;
    map.record(1, {.original_file = "a.glsl",
                   .original_line = 1,
                   .rewrite_applied = false,
                   .rewrite_description = {}});
    map.record(1, {.original_file = "b.glsl",
                   .original_line = 99,
                   .rewrite_applied = false,
                   .rewrite_description = {}});

    REQUIRE(map.size() == 1);
    auto* entry = map.lookup(1);
    REQUIRE(entry != nullptr);
    REQUIRE(entry->original_file == "b.glsl");
    REQUIRE(entry->original_line == 99);
}

TEST_CASE("RetroArchPreprocessor provenance tracking with includes", "[diagnostics][provenance]") {
    // Create temporary include file
    auto temp_dir = std::filesystem::temp_directory_path() / "goggles_provenance_test";
    std::filesystem::create_directories(temp_dir);

    auto include_path = temp_dir / "common.glsl";
    {
        std::ofstream f(include_path);
        f << "// included line 1\n";
        f << "// included line 2\n";
    }

    auto main_source = "#include \"common.glsl\"\n"
                       "// main line after include\n";

    goggles::render::RetroArchPreprocessor preprocessor;
    SourceProvenanceMap provenance;
    auto result = preprocessor.preprocess_source(main_source, temp_dir, &provenance);
    REQUIRE(result.has_value());
    REQUIRE(provenance.size() > 0);

    // Included lines should have provenance entries pointing to common.glsl
    bool found_include_provenance = false;
    for (uint32_t i = 1; i <= 10; ++i) {
        auto* entry = provenance.lookup(i);
        if (entry != nullptr && entry->original_file.find("common.glsl") != std::string::npos) {
            found_include_provenance = true;
            break;
        }
    }
    REQUIRE(found_include_provenance);

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("RetroArchPreprocessor without provenance still works", "[diagnostics][provenance]") {
    goggles::render::RetroArchPreprocessor preprocessor;
    std::string source = R"(
#pragma stage vertex
void main() { gl_Position = vec4(0.0); }

#pragma stage fragment
void main() { FragColor = vec4(1.0); }
)";

    auto result = preprocessor.preprocess_source(source, "");
    REQUIRE(result.has_value());
    REQUIRE(!result->vertex_source.empty());
}

TEST_CASE("RetroArchPreprocessor provenance tracks compatibility rewrites",
          "[diagnostics][provenance]") {
    goggles::render::RetroArchPreprocessor preprocessor;
    std::string source = R"(
#pragma stage vertex
void main() { gl_Position = vec4(0.0); }

#pragma stage fragment
layout(location = 0) out vec4 FragColor;
#define mix_mat mat3(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
void main() {
    vec3 yiq = vec3(1.0);
    yiq *= mix_mat;
    FragColor = vec4(yiq, 1.0);
}
)";

    SourceProvenanceMap provenance;
    auto result = preprocessor.preprocess_source(source, "", &provenance);
    REQUIRE(result.has_value());

    // Check that at least one entry has rewrite_applied
    bool found_rewrite = false;
    for (uint32_t i = 1; i <= 20; ++i) {
        auto* entry = provenance.lookup(i);
        if (entry != nullptr && entry->rewrite_applied) {
            found_rewrite = true;
            REQUIRE(entry->rewrite_description.find("Slang") != std::string::npos);
            break;
        }
    }
    REQUIRE(found_rewrite);
}
