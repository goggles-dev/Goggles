#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <util/diagnostics/diagnostic_event.hpp>
#include <util/diagnostics/diagnostic_policy.hpp>
#include <util/diagnostics/diagnostic_session.hpp>
#include <util/diagnostics/test_harness_sink.hpp>

using namespace goggles::diagnostics;

namespace {

class ThrowingSink : public DiagnosticSink {
public:
    void receive(const DiagnosticEvent& event) override {
        static_cast<void>(event);
        throw std::runtime_error("sink failure");
    }
};

} // namespace

TEST_CASE("Session with no sinks silently discards events", "[diagnostics][session]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});
    DiagnosticEvent event;
    event.severity = Severity::info;
    event.message = "test";

    REQUIRE_NOTHROW(session->emit(event));
    REQUIRE(session->event_count(Severity::info) == 1);
}

TEST_CASE("Session with one sink delivers events", "[diagnostics][session]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});
    auto sink = std::make_unique<TestHarnessSink>();
    auto* sink_ptr = sink.get();
    session->register_sink(std::move(sink));

    DiagnosticEvent event;
    event.severity = Severity::warning;
    event.category = Category::runtime;
    event.message = "binding fallback";
    session->emit(event);

    REQUIRE(sink_ptr->event_count() == 1);
    REQUIRE(sink_ptr->all_events()[0].message == "binding fallback");
}

TEST_CASE("Session with two sinks delivers to both", "[diagnostics][session]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});
    auto sink1 = std::make_unique<TestHarnessSink>();
    auto sink2 = std::make_unique<TestHarnessSink>();
    auto* ptr1 = sink1.get();
    auto* ptr2 = sink2.get();
    session->register_sink(std::move(sink1));
    session->register_sink(std::move(sink2));

    DiagnosticEvent event;
    event.message = "broadcast";
    session->emit(event);

    REQUIRE(ptr1->event_count() == 1);
    REQUIRE(ptr2->event_count() == 1);
}

TEST_CASE("Session severity promotion in strict mode", "[diagnostics][session]") {
    auto policy = make_strict_policy();
    auto session = DiagnosticSession::create(policy);
    auto sink = std::make_unique<TestHarnessSink>();
    auto* sink_ptr = sink.get();
    session->register_sink(std::move(sink));

    DiagnosticEvent event;
    event.severity = Severity::warning;
    event.category = Category::runtime;
    event.evidence = BindingEvidence{.resource_id = "tex",
                                     .is_fallback = true,
                                     .width = 0,
                                     .height = 0,
                                     .format = 0,
                                     .producer_pass = LocalizationKey::CHAIN_LEVEL,
                                     .alias_name = {}};
    event.message = "fallback substitution";
    session->emit(event);

    REQUIRE(sink_ptr->event_count() == 1);
    const auto& received = sink_ptr->all_events()[0];
    REQUIRE(received.severity == Severity::error);
    REQUIRE(received.original_severity == Severity::warning);
}

TEST_CASE("Session event counting by severity and category", "[diagnostics][session]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});

    DiagnosticEvent e1;
    e1.severity = Severity::info;
    e1.category = Category::authoring;
    session->emit(e1);

    DiagnosticEvent e2;
    e2.severity = Severity::warning;
    e2.category = Category::runtime;
    session->emit(e2);

    DiagnosticEvent e3;
    e3.severity = Severity::info;
    e3.category = Category::runtime;
    session->emit(e3);

    REQUIRE(session->event_count(Severity::info) == 2);
    REQUIRE(session->event_count(Severity::warning) == 1);
    REQUIRE(session->event_count(Severity::error) == 0);
    REQUIRE(session->event_count(Category::authoring) == 1);
    REQUIRE(session->event_count(Category::runtime) == 2);
}

TEST_CASE("Session begin_frame/end_frame tracking", "[diagnostics][session]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});

    session->begin_frame(0);
    REQUIRE(session->identity().frame_start == 0);
    REQUIRE(session->identity().frame_end == 0);

    session->begin_frame(5);
    REQUIRE(session->identity().frame_start == 0);
    REQUIRE(session->identity().frame_end == 5);
    session->end_frame();
}

TEST_CASE("Session reset clears all state", "[diagnostics][session]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});

    DiagnosticEvent event;
    event.severity = Severity::error;
    event.category = Category::authoring;
    session->emit(event);

    session->set_authoring_verdict(AuthoringVerdict{.result = VerdictResult::pass, .findings = {}});
    session->begin_frame(10);

    REQUIRE(session->event_count(Severity::error) == 1);
    REQUIRE(session->authoring_verdict().has_value());

    session->reset();

    REQUIRE(session->event_count(Severity::error) == 0);
    REQUIRE_FALSE(session->authoring_verdict().has_value());
    REQUIRE(session->identity().frame_start == 0);
}

TEST_CASE("Session unregister_sink removes sink", "[diagnostics][session]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});

    auto sink1 = std::make_unique<TestHarnessSink>();
    auto* sink1_ptr = sink1.get();
    auto id1 = session->register_sink(std::move(sink1));

    auto sink2 = std::make_unique<TestHarnessSink>();
    auto* sink2_ptr = sink2.get();
    session->register_sink(std::move(sink2));

    DiagnosticEvent event;
    event.message = "before unregister";
    session->emit(event);
    REQUIRE(sink1_ptr->event_count() == 1);
    REQUIRE(sink2_ptr->event_count() == 1);

    session->unregister_sink(id1);

    DiagnosticEvent event2;
    event2.message = "after unregister";
    session->emit(event2);
    // sink1 was removed, sink2 still receives
    REQUIRE(sink2_ptr->event_count() == 2);
}

TEST_CASE("Session attaches identity to emitted events", "[diagnostics][session]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});
    auto sink = std::make_unique<TestHarnessSink>();
    auto* sink_ptr = sink.get();
    session->register_sink(std::move(sink));

    SessionIdentity identity{};
    identity.generation_id = 42;
    identity.capture_mode = "standard";
    session->update_identity(identity);

    DiagnosticEvent event;
    event.message = "identity linked";
    session->emit(event);

    REQUIRE(sink_ptr->event_count() == 1);
    REQUIRE(sink_ptr->all_events()[0].session_identity != nullptr);
    REQUIRE(sink_ptr->all_events()[0].session_identity->generation_id == 42);
}

TEST_CASE("Session self-reports sink failures", "[diagnostics][session]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});
    auto healthy_sink = std::make_unique<TestHarnessSink>();
    auto* healthy_sink_ptr = healthy_sink.get();
    session->register_sink(std::move(healthy_sink));
    session->register_sink(std::make_unique<ThrowingSink>());

    DiagnosticEvent event;
    event.message = "primary event";
    session->emit(event);

    REQUIRE(healthy_sink_ptr->event_count() == 2);
    REQUIRE(healthy_sink_ptr->all_events()[0].message == "primary event");
    REQUIRE(healthy_sink_ptr->all_events()[1].localization.stage == "sink");
    REQUIRE(healthy_sink_ptr->all_events()[1].severity == Severity::warning);
}
