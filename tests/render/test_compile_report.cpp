#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <util/diagnostics/compile_report.hpp>

using namespace goggles::diagnostics;

TEST_CASE("CompileReport empty by default", "[diagnostics][compile_report]") {
    CompileReport report;
    REQUIRE(report.stages().empty());
    REQUIRE_FALSE(report.all_succeeded());
    REQUIRE_THAT(report.total_timing_us(), Catch::Matchers::WithinAbs(0.0, 0.001));
}

TEST_CASE("CompileReport tracks successful stages", "[diagnostics][compile_report]") {
    CompileReport report;
    report.add_stage({.stage = CompileStage::vertex,
                      .success = true,
                      .messages = {},
                      .timing_us = 100.0,
                      .cache_hit = false});
    report.add_stage({.stage = CompileStage::fragment,
                      .success = true,
                      .messages = {},
                      .timing_us = 200.0,
                      .cache_hit = false});

    REQUIRE(report.stages().size() == 2);
    REQUIRE(report.all_succeeded());
    REQUIRE_THAT(report.total_timing_us(), Catch::Matchers::WithinAbs(300.0, 0.001));
}

TEST_CASE("CompileReport tracks failed stages", "[diagnostics][compile_report]") {
    CompileReport report;
    report.add_stage({.stage = CompileStage::vertex,
                      .success = true,
                      .messages = {},
                      .timing_us = 50.0,
                      .cache_hit = false});
    report.add_stage({.stage = CompileStage::fragment,
                      .success = false,
                      .messages = {"error: undeclared identifier 'foo'"},
                      .timing_us = 10.0,
                      .cache_hit = false});

    REQUIRE(report.stages().size() == 2);
    REQUIRE_FALSE(report.all_succeeded());
    REQUIRE_THAT(report.total_timing_us(), Catch::Matchers::WithinAbs(60.0, 0.001));

    REQUIRE(report.stages()[1].messages.size() == 1);
    REQUIRE(report.stages()[1].messages[0].find("foo") != std::string::npos);
}

TEST_CASE("CompileReport tracks cache hits", "[diagnostics][compile_report]") {
    CompileReport report;
    report.add_stage({.stage = CompileStage::vertex,
                      .success = true,
                      .messages = {},
                      .timing_us = 0.0,
                      .cache_hit = true});
    report.add_stage({.stage = CompileStage::fragment,
                      .success = true,
                      .messages = {},
                      .timing_us = 0.0,
                      .cache_hit = true});

    REQUIRE(report.all_succeeded());
    REQUIRE(report.stages()[0].cache_hit);
    REQUIRE(report.stages()[1].cache_hit);
}

TEST_CASE("CompileReport stage enum values", "[diagnostics][compile_report]") {
    REQUIRE(static_cast<uint8_t>(CompileStage::vertex) == 0);
    REQUIRE(static_cast<uint8_t>(CompileStage::fragment) == 1);
}
