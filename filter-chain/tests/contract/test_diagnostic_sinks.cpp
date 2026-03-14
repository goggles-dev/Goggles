#include "diagnostics/diagnostic_event.hpp"
#include "diagnostics/log_sink.hpp"
#include "diagnostics/test_harness_sink.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace goggles::diagnostics;

TEST_CASE("TestHarnessSink collects events in emission order", "[diagnostics][sink]") {
    TestHarnessSink sink;
    REQUIRE(sink.event_count() == 0);

    DiagnosticEvent e1;
    e1.severity = Severity::info;
    e1.category = Category::authoring;
    e1.message = "first";
    sink.receive(e1);

    DiagnosticEvent e2;
    e2.severity = Severity::warning;
    e2.category = Category::runtime;
    e2.message = "second";
    sink.receive(e2);

    REQUIRE(sink.event_count() == 2);
    REQUIRE(sink.all_events()[0].message == "first");
    REQUIRE(sink.all_events()[1].message == "second");
}

TEST_CASE("TestHarnessSink filter by category", "[diagnostics][sink]") {
    TestHarnessSink sink;

    DiagnosticEvent authoring_event;
    authoring_event.category = Category::authoring;
    sink.receive(authoring_event);

    DiagnosticEvent runtime_event;
    runtime_event.category = Category::runtime;
    sink.receive(runtime_event);

    DiagnosticEvent runtime_event2;
    runtime_event2.category = Category::runtime;
    sink.receive(runtime_event2);

    auto authoring = sink.events_by_category(Category::authoring);
    REQUIRE(authoring.size() == 1);

    auto runtime = sink.events_by_category(Category::runtime);
    REQUIRE(runtime.size() == 2);

    auto quality = sink.events_by_category(Category::quality);
    REQUIRE(quality.empty());
}

TEST_CASE("TestHarnessSink filter by severity", "[diagnostics][sink]") {
    TestHarnessSink sink;

    DiagnosticEvent warn;
    warn.severity = Severity::warning;
    sink.receive(warn);

    DiagnosticEvent err;
    err.severity = Severity::error;
    sink.receive(err);

    REQUIRE(sink.events_by_severity(Severity::warning).size() == 1);
    REQUIRE(sink.events_by_severity(Severity::error).size() == 1);
    REQUIRE(sink.events_by_severity(Severity::debug).empty());
}

TEST_CASE("TestHarnessSink clear resets state", "[diagnostics][sink]") {
    TestHarnessSink sink;
    DiagnosticEvent event;
    sink.receive(event);
    REQUIRE(sink.event_count() == 1);

    sink.clear();
    REQUIRE(sink.event_count() == 0);
}

TEST_CASE("LogSink does not throw on receive", "[diagnostics][sink]") {
    LogSink sink;
    DiagnosticEvent event;
    event.severity = Severity::info;
    event.category = Category::authoring;
    event.message = "test message";

    REQUIRE_NOTHROW(sink.receive(event));
}

TEST_CASE("LogSink handles chain-level events", "[diagnostics][sink]") {
    LogSink sink;
    DiagnosticEvent event;
    event.severity = Severity::warning;
    event.category = Category::runtime;
    event.localization.pass_ordinal = LocalizationKey::CHAIN_LEVEL;
    event.message = "chain-level warning";

    REQUIRE_NOTHROW(sink.receive(event));
}

TEST_CASE("LogSink handles pass-level events", "[diagnostics][sink]") {
    LogSink sink;
    DiagnosticEvent event;
    event.severity = Severity::error;
    event.category = Category::runtime;
    event.localization.pass_ordinal = 3;
    event.message = "pass 3 error";

    REQUIRE_NOTHROW(sink.receive(event));
}
