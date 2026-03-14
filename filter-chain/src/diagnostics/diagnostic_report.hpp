#pragma once

#include "diagnostic_session.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

namespace goggles::diagnostics {

struct ErrorCountsByCategory {
    uint32_t authoring = 0;
    uint32_t runtime = 0;
    uint32_t quality = 0;
    uint32_t capture = 0;
};

struct BindingCoverageRow {
    uint32_t pass_ordinal = 0;
    uint32_t resolved = 0;
    uint32_t substituted = 0;
    uint32_t unresolved = 0;
};

struct SemanticCoverageRow {
    uint32_t pass_ordinal = 0;
    uint32_t parameter = 0;
    uint32_t semantic = 0;
    uint32_t statik = 0;
    uint32_t unresolved = 0;
};

struct DiagnosticReport {
    CaptureMode reporting_mode = CaptureMode::standard;
    SessionIdentity session_identity;
    std::optional<AuthoringVerdict> authoring_verdict;
    bool degraded = false;
    ErrorCountsByCategory error_counts_by_category;
    std::optional<ChainManifest> manifest;
    std::vector<CompileEvidence> compile_summaries;
    std::vector<ReflectionEvidence> reflection_summaries;
    std::vector<BindingCoverageRow> binding_coverage;
    std::vector<SemanticCoverageRow> semantic_coverage;
    std::vector<TimelineEvent> execution_trace;
    std::vector<ProvenanceEvidence> provenance;
    std::vector<DegradationEntry> degradation_entries;
    std::vector<CaptureEvidence> selected_captures;
    std::vector<DiagnosticEvent> event_timeline;
    bool includes_artifact_bundle = false;
};

[[nodiscard]] inline auto build_diagnostic_report(const DiagnosticSession& session)
    -> DiagnosticReport {
    DiagnosticReport report{};
    report.reporting_mode = session.policy().capture_mode;
    report.session_identity = session.identity();
    report.authoring_verdict = session.authoring_verdict();
    report.degraded = !session.degradation_ledger().all_entries().empty() ||
                      (report.authoring_verdict.has_value() &&
                       report.authoring_verdict->result == VerdictResult::degraded);

    const auto accumulate_error = [&](Category category) {
        switch (category) {
        case Category::authoring:
            ++report.error_counts_by_category.authoring;
            break;
        case Category::runtime:
            ++report.error_counts_by_category.runtime;
            break;
        case Category::quality:
            ++report.error_counts_by_category.quality;
            break;
        case Category::capture:
            ++report.error_counts_by_category.capture;
            break;
        }
    };

    for (const auto& event : session.events()) {
        if (event.severity == Severity::error) {
            accumulate_error(event.category);
        }
    }

    if (report.reporting_mode == CaptureMode::minimal) {
        return report;
    }

    if (const auto* manifest = session.chain_manifest(); manifest != nullptr) {
        report.manifest = *manifest;
    }

    std::unordered_map<uint32_t, BindingCoverageRow> binding_rows;
    for (const auto& entry : session.binding_ledger().all_entries()) {
        auto& row = binding_rows[entry.pass_ordinal];
        row.pass_ordinal = entry.pass_ordinal;
        switch (entry.status) {
        case BindingStatus::resolved:
            ++row.resolved;
            break;
        case BindingStatus::substituted:
            ++row.substituted;
            break;
        case BindingStatus::unresolved:
            ++row.unresolved;
            break;
        }
    }
    for (const auto& [_, row] : binding_rows) {
        report.binding_coverage.push_back(row);
    }
    std::sort(report.binding_coverage.begin(), report.binding_coverage.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.pass_ordinal < rhs.pass_ordinal; });

    std::unordered_map<uint32_t, SemanticCoverageRow> semantic_rows;
    for (const auto& entry : session.semantic_ledger().all_entries()) {
        auto& row = semantic_rows[entry.pass_ordinal];
        row.pass_ordinal = entry.pass_ordinal;
        switch (entry.classification) {
        case SemanticClassification::parameter:
            ++row.parameter;
            break;
        case SemanticClassification::semantic:
            ++row.semantic;
            break;
        case SemanticClassification::static_value:
            ++row.statik;
            break;
        case SemanticClassification::unresolved:
            ++row.unresolved;
            break;
        }
    }
    for (const auto& [_, row] : semantic_rows) {
        report.semantic_coverage.push_back(row);
    }
    std::sort(report.semantic_coverage.begin(), report.semantic_coverage.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.pass_ordinal < rhs.pass_ordinal; });

    report.execution_trace.assign(session.execution_timeline().events().begin(),
                                  session.execution_timeline().events().end());

    for (const auto& event : session.events()) {
        if (const auto* compile = std::get_if<CompileEvidence>(&event.evidence)) {
            report.compile_summaries.push_back(*compile);
        }
        if (const auto* reflection = std::get_if<ReflectionEvidence>(&event.evidence)) {
            report.reflection_summaries.push_back(*reflection);
        }
    }

    if (report.reporting_mode == CaptureMode::standard) {
        return report;
    }

    report.degradation_entries.assign(session.degradation_ledger().all_entries().begin(),
                                      session.degradation_ledger().all_entries().end());
    for (const auto& event : session.events()) {
        if (const auto* provenance = std::get_if<ProvenanceEvidence>(&event.evidence)) {
            report.provenance.push_back(*provenance);
        }
        if (const auto* capture = std::get_if<CaptureEvidence>(&event.evidence)) {
            report.selected_captures.push_back(*capture);
        }
    }

    if (report.reporting_mode == CaptureMode::investigate) {
        return report;
    }

    report.event_timeline.assign(session.events().begin(), session.events().end());
    report.includes_artifact_bundle = true;
    return report;
}

} // namespace goggles::diagnostics
