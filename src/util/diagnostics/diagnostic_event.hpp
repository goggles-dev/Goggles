#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <util/diagnostics/session_identity.hpp>
#include <variant>
#include <vector>

namespace goggles::diagnostics {

enum class Severity : uint8_t { debug, info, warning, error };

enum class Category : uint8_t { authoring, runtime, quality, capture };

struct LocalizationKey {
    static constexpr uint32_t CHAIN_LEVEL = UINT32_MAX;
    uint32_t pass_ordinal = CHAIN_LEVEL;
    std::string_view stage;
    std::string_view resource;
};

struct BindingEvidence {
    std::string resource_id;
    bool is_fallback = false;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint32_t producer_pass = LocalizationKey::CHAIN_LEVEL;
    std::string alias_name;
};

struct SemanticEvidence {
    std::string member_name;
    std::string classification;
    std::variant<float, std::array<float, 4>> value;
    uint32_t offset = 0;
};

struct CompileEvidence {
    std::string stage;
    bool success = false;
    std::vector<std::string> messages;
    double timing_us = 0.0;
    bool cache_hit = false;
};

struct ReflectionEvidence {
    std::string stage;
    std::vector<std::string> resource_summary;
    std::vector<std::string> merge_conflicts;
};

struct ProvenanceEvidence {
    std::string original_file;
    uint32_t original_line = 0;
    bool rewrite_applied = false;
    std::string rewrite_description;
};

struct CaptureEvidence {
    uint32_t pass_ordinal = 0;
    uint32_t frame_index = 0;
    std::string image_ref;
};

using EvidencePayload =
    std::variant<std::monostate, BindingEvidence, SemanticEvidence, CompileEvidence,
                 ReflectionEvidence, ProvenanceEvidence, CaptureEvidence>;

struct DiagnosticEvent {
    Severity severity = Severity::info;
    Severity original_severity = Severity::info;
    Category category = Category::runtime;
    LocalizationKey localization;
    uint32_t frame_index = 0;
    uint64_t timestamp_ns = 0;
    const SessionIdentity* session_identity = nullptr;
    std::string message;
    EvidencePayload evidence;
};

/// @brief Verdict summarizing authoring validation results.
enum class VerdictResult : uint8_t { pass, degraded, fail };

struct AuthoringFinding {
    Severity severity = Severity::info;
    LocalizationKey localization;
    std::string message;
};

struct AuthoringVerdict {
    VerdictResult result = VerdictResult::pass;
    std::vector<AuthoringFinding> findings;
};

} // namespace goggles::diagnostics
