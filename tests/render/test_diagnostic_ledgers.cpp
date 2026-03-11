#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <util/diagnostics/chain_manifest.hpp>
#include <util/diagnostics/degradation_ledger.hpp>
#include <util/diagnostics/semantic_ledger.hpp>

using namespace goggles::diagnostics;
using Catch::Approx;

TEST_CASE("DegradationLedger records per-pass degradations in frame order",
          "[diagnostics][ledger]") {
    DegradationLedger ledger;

    ledger.record({.pass_ordinal = 1,
                   .expected_resource = "History1",
                   .substituted_resource = "Source",
                   .frame_index = 2,
                   .type = DegradationType::texture_fallback});
    ledger.record({.pass_ordinal = 0,
                   .expected_resource = "OutputSize",
                   .substituted_resource = "",
                   .frame_index = 1,
                   .type = DegradationType::semantic_unresolved});
    ledger.record({.pass_ordinal = 1,
                   .expected_resource = "Reflection",
                   .substituted_resource = "",
                   .frame_index = 5,
                   .type = DegradationType::reflection_loss});

    REQUIRE(ledger.all_entries().size() == 3);

    const auto pass0 = ledger.entries_for_pass(0);
    REQUIRE(pass0.size() == 1);
    CHECK(pass0.front().type == DegradationType::semantic_unresolved);
    CHECK(pass0.front().frame_index == 1);

    const auto pass1 = ledger.entries_for_pass(1);
    REQUIRE(pass1.size() == 2);
    CHECK(pass1.front().frame_index == 2);
    CHECK(pass1.back().frame_index == 5);

    ledger.clear();
    CHECK(ledger.all_entries().empty());
}

TEST_CASE("SemanticAssignmentLedger records scalar and vector semantics", "[diagnostics][ledger]") {
    SemanticAssignmentLedger ledger;

    ledger.record({.pass_ordinal = 2,
                   .member_name = "FrameCount",
                   .classification = SemanticClassification::semantic,
                   .value = 3.0F,
                   .offset = 16});
    ledger.record({.pass_ordinal = 2,
                   .member_name = "SourceSize",
                   .classification = SemanticClassification::parameter,
                   .value = std::array<float, 4>{64.0F, 64.0F, 1.0F / 64.0F, 1.0F / 64.0F},
                   .offset = 32});

    const auto pass2 = ledger.entries_for_pass(2);
    REQUIRE(pass2.size() == 2);
    CHECK(pass2.front().classification == SemanticClassification::semantic);
    CHECK(std::get<float>(pass2.front().value) == 3.0F);

    const auto vector_value = std::get<std::array<float, 4>>(pass2.back().value);
    CHECK(vector_value[0] == 64.0F);
    CHECK(vector_value[2] == Approx(1.0F / 64.0F));

    ledger.clear();
    CHECK(ledger.all_entries().empty());
}

TEST_CASE("ChainManifest preserves deterministic insertion order", "[diagnostics][manifest]") {
    ChainManifest manifest;
    manifest.add_pass({.ordinal = 0,
                       .shader_path = "first.slang",
                       .scale_type_x = "source",
                       .scale_type_y = "source",
                       .scale_x = 1.0F,
                       .scale_y = 1.0F,
                       .format_override = "",
                       .wrap_mode = "clamp_to_edge",
                       .alias = "First"});
    manifest.add_pass({.ordinal = 1,
                       .shader_path = "second.slang",
                       .scale_type_x = "viewport",
                       .scale_type_y = "viewport",
                       .scale_x = 2.0F,
                       .scale_y = 2.0F,
                       .format_override = "rgba8",
                       .wrap_mode = "repeat",
                       .alias = "Second"});
    manifest.add_alias("First");
    manifest.add_alias("Second");
    manifest.add_texture({.name = "LUT",
                          .path = "lut.png",
                          .filter_mode = "linear",
                          .mipmap = false,
                          .wrap_mode = "clamp_to_edge"});
    manifest.set_temporal(
        {.history_depth = 2, .feedback_producer_passes = {1}, .feedback_consumer_passes = {0, 1}});

    REQUIRE(manifest.passes().size() == 2);
    CHECK(manifest.passes()[0].shader_path == "first.slang");
    CHECK(manifest.passes()[1].shader_path == "second.slang");
    CHECK(manifest.aliases()[0] == "First");
    CHECK(manifest.aliases()[1] == "Second");
    REQUIRE(manifest.textures().size() == 1);
    CHECK(manifest.temporal().history_depth == 2);
    CHECK(manifest.temporal().feedback_producer_passes == std::vector<uint32_t>{1});
}
