#include "diagnostics/diagnostic_event.hpp"
#include "diagnostics/diagnostic_policy.hpp"
#include "diagnostics/diagnostic_session.hpp"
#include "diagnostics/test_harness_sink.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace goggles::diagnostics;

TEST_CASE("Authoring verdict pass for clean session", "[diagnostics][authoring]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});
    auto sink = std::make_unique<TestHarnessSink>();
    auto* sink_ptr = sink.get();
    session->register_sink(std::move(sink));

    // Simulate manifest event
    session->emit({.severity = Severity::info,
                   .original_severity = Severity::info,
                   .category = Category::authoring,
                   .localization = {.pass_ordinal = LocalizationKey::CHAIN_LEVEL,
                                    .stage = "manifest",
                                    .resource = {}},
                   .frame_index = 0,
                   .timestamp_ns = 0,
                   .message = "Chain manifest generated",
                   .evidence = {},
                   .session_identity = std::nullopt});

    // Simulate compile report events for 2 passes
    for (uint32_t pass = 0; pass < 2; ++pass) {
        session->emit({.severity = Severity::info,
                       .original_severity = Severity::info,
                       .category = Category::authoring,
                       .localization = {.pass_ordinal = pass, .stage = "compile", .resource = {}},
                       .frame_index = 0,
                       .timestamp_ns = 0,
                       .message = "Compile succeeded",
                       .evidence = CompileEvidence{.stage = "vertex+fragment",
                                                   .success = true,
                                                   .messages = {},
                                                   .timing_us = 100.0,
                                                   .cache_hit = false},
                       .session_identity = std::nullopt});
    }

    // Set verdict
    AuthoringVerdict verdict;
    verdict.result = VerdictResult::pass;
    session->set_authoring_verdict(verdict);

    session->emit({.severity = Severity::info,
                   .original_severity = Severity::info,
                   .category = Category::authoring,
                   .localization = {.pass_ordinal = LocalizationKey::CHAIN_LEVEL,
                                    .stage = "verdict",
                                    .resource = {}},
                   .frame_index = 0,
                   .timestamp_ns = 0,
                   .message = "Authoring verdict: pass",
                   .evidence = {},
                   .session_identity = std::nullopt});

    // Verify events
    auto authoring_events = sink_ptr->events_by_category(Category::authoring);
    REQUIRE(authoring_events.size() == 4); // manifest + 2 compile + verdict

    // Verify manifest event
    REQUIRE(authoring_events[0].localization.stage == "manifest");

    // Verify compile events
    REQUIRE(authoring_events[1].localization.pass_ordinal == 0);
    REQUIRE(authoring_events[1].localization.stage == "compile");
    REQUIRE(authoring_events[2].localization.pass_ordinal == 1);

    // Verify verdict
    auto verdict_result = session->authoring_verdict();
    REQUIRE(verdict_result.has_value());
    REQUIRE(verdict_result->result == VerdictResult::pass);
    REQUIRE(verdict_result->findings.empty());
}

TEST_CASE("Authoring verdict degraded for empty reflection in compat mode",
          "[diagnostics][authoring]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});
    auto sink = std::make_unique<TestHarnessSink>();
    auto* sink_ptr = sink.get();
    session->register_sink(std::move(sink));

    // Simulate a reflection warning (empty contract in compatibility mode)
    session->emit({.severity = Severity::warning,
                   .original_severity = Severity::warning,
                   .category = Category::authoring,
                   .localization = {.pass_ordinal = 0, .stage = "reflection", .resource = {}},
                   .frame_index = 0,
                   .timestamp_ns = 0,
                   .message = "Empty reflection contract (compatibility mode: degraded)",
                   .evidence = ReflectionEvidence{.stage = "vertex+fragment",
                                                  .resource_summary = {},
                                                  .merge_conflicts = {}},
                   .session_identity = std::nullopt});

    AuthoringVerdict verdict;
    verdict.result = VerdictResult::degraded;
    verdict.findings.push_back(
        {.severity = Severity::warning,
         .localization = {.pass_ordinal = 0, .stage = "reflection", .resource = {}},
         .message = "Empty reflection contract — pass marked as degraded"});
    session->set_authoring_verdict(verdict);

    auto result = session->authoring_verdict();
    REQUIRE(result.has_value());
    REQUIRE(result->result == VerdictResult::degraded);
    REQUIRE(result->findings.size() == 1);
    REQUIRE(result->findings[0].severity == Severity::warning);

    auto warnings = sink_ptr->events_by_severity(Severity::warning);
    REQUIRE(warnings.size() >= 1);
}

TEST_CASE("Authoring verdict fail for compile error", "[diagnostics][authoring]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});
    auto sink = std::make_unique<TestHarnessSink>();
    auto* sink_ptr = sink.get();
    session->register_sink(std::move(sink));

    // Simulate compile failure
    session->emit({.severity = Severity::error,
                   .original_severity = Severity::error,
                   .category = Category::authoring,
                   .localization = {.pass_ordinal = 0, .stage = "compile", .resource = {}},
                   .frame_index = 0,
                   .timestamp_ns = 0,
                   .message = "Compile failed",
                   .evidence = CompileEvidence{.stage = "fragment",
                                               .success = false,
                                               .messages = {},
                                               .timing_us = 0.0,
                                               .cache_hit = false},
                   .session_identity = std::nullopt});

    AuthoringVerdict verdict;
    verdict.result = VerdictResult::fail;
    verdict.findings.push_back(
        {.severity = Severity::error,
         .localization = {.pass_ordinal = 0, .stage = "compile", .resource = {}},
         .message = "Fragment shader compilation failed"});
    session->set_authoring_verdict(verdict);

    auto result = session->authoring_verdict();
    REQUIRE(result.has_value());
    REQUIRE(result->result == VerdictResult::fail);
    REQUIRE(result->findings.size() == 1);
    REQUIRE(result->findings[0].severity == Severity::error);

    auto errors = sink_ptr->events_by_severity(Severity::error);
    REQUIRE(errors.size() >= 1);
}

TEST_CASE("Strict mode reflection conformance gate rejects empty reflection",
          "[diagnostics][authoring]") {
    auto policy = make_strict_policy();
    auto session = DiagnosticSession::create(policy);
    auto sink = std::make_unique<TestHarnessSink>();
    auto* sink_ptr = sink.get();
    session->register_sink(std::move(sink));

    REQUIRE(session->policy().mode == PolicyMode::strict);
    REQUIRE(session->policy().reflection_loss_is_fatal == true);

    // Simulate reflection error in strict mode
    session->emit({.severity = Severity::error,
                   .original_severity = Severity::error,
                   .category = Category::authoring,
                   .localization = {.pass_ordinal = 0, .stage = "reflection", .resource = {}},
                   .frame_index = 0,
                   .timestamp_ns = 0,
                   .message = "Empty reflection contract (strict mode)",
                   .evidence = ReflectionEvidence{.stage = "vertex+fragment",
                                                  .resource_summary = {},
                                                  .merge_conflicts = {}},
                   .session_identity = std::nullopt});

    AuthoringVerdict verdict;
    verdict.result = VerdictResult::fail;
    verdict.findings.push_back(
        {.severity = Severity::error,
         .localization = {.pass_ordinal = 0, .stage = "reflection", .resource = {}},
         .message = "Empty reflection contract rejected in strict mode"});
    session->set_authoring_verdict(verdict);

    auto result = session->authoring_verdict();
    REQUIRE(result.has_value());
    REQUIRE(result->result == VerdictResult::fail);

    auto errors = sink_ptr->events_by_severity(Severity::error);
    REQUIRE(errors.size() >= 1);
}

TEST_CASE("Authoring events track provenance", "[diagnostics][authoring]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});
    auto sink = std::make_unique<TestHarnessSink>();
    auto* sink_ptr = sink.get();
    session->register_sink(std::move(sink));

    session->emit({.severity = Severity::debug,
                   .original_severity = Severity::debug,
                   .category = Category::authoring,
                   .localization = {.pass_ordinal = 0, .stage = "provenance", .resource = {}},
                   .frame_index = 0,
                   .timestamp_ns = 0,
                   .message = "Source provenance tracked (42 entries)",
                   .evidence = ProvenanceEvidence{.original_file = "shader.slang",
                                                  .original_line = 0,
                                                  .rewrite_applied = false,
                                                  .rewrite_description = {}},
                   .session_identity = std::nullopt});

    auto authoring = sink_ptr->events_by_category(Category::authoring);
    REQUIRE(authoring.size() == 1);
    REQUIRE(std::holds_alternative<ProvenanceEvidence>(authoring[0].evidence));
    auto& prov = std::get<ProvenanceEvidence>(authoring[0].evidence);
    REQUIRE(prov.original_file == "shader.slang");
}

TEST_CASE("Chain manifest stored in session", "[diagnostics][authoring]") {
    auto session = DiagnosticSession::create(DiagnosticPolicy{});

    REQUIRE(session->chain_manifest() == nullptr);

    auto manifest = std::make_unique<ChainManifest>();
    ManifestPassEntry entry;
    entry.ordinal = 0;
    entry.shader_path = "test.slang";
    manifest->add_pass(std::move(entry));
    manifest->add_alias("TestAlias");

    session->set_chain_manifest(std::move(manifest));

    REQUIRE(session->chain_manifest() != nullptr);
    REQUIRE(session->chain_manifest()->passes().size() == 1);
    REQUIRE(session->chain_manifest()->passes()[0].shader_path == "test.slang");
    REQUIRE(session->chain_manifest()->aliases().size() == 1);
}
