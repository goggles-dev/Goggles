#include "shader_batch_report.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <render/chain/preset_parser.hpp>
#include <render/shader/retroarch_preprocessor.hpp>
#include <render/shader/shader_runtime.hpp>
#include <render/shader/slang_reflect.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <util/diagnostics/compile_report.hpp>
#include <util/diagnostics/source_provenance.hpp>

namespace goggles::test {

namespace {

struct BatchPassResult {
    diagnostics::CompileReport compile_report;
    std::vector<std::string> reflection_summary;
    std::vector<std::string> parameter_names;
    std::vector<std::string> semantic_findings;
    bool compile_ok = false;
    std::optional<std::string> error;
};

struct BatchPresetResult {
    std::filesystem::path path;
    bool parse_ok = false;
    bool compile_ok = false;
    std::optional<std::string> error;
    std::string authoring_verdict = "fail";
    std::vector<BatchPassResult> pass_results;
    std::vector<std::string> conformance_findings;
};

struct BatchSummary {
    size_t total = 0;
    size_t passed = 0;
    size_t failed = 0;
    size_t skipped = 0;
    size_t degraded = 0;
};

auto json_escape(std::string_view text) -> std::string {
    std::string out;
    out.reserve(text.size() + 8);
    for (const char ch : text) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

auto stage_name(diagnostics::CompileStage stage) -> const char* {
    switch (stage) {
    case diagnostics::CompileStage::vertex:
        return "vertex";
    case diagnostics::CompileStage::fragment:
        return "fragment";
    }
    return "unknown";
}

auto semantic_name(std::string_view name) -> bool {
    return name == "MVP" || name == "SourceSize" || name == "OutputSize" ||
           name == "OriginalSize" || name == "FinalViewportSize" || name == "FrameCount" ||
           (name.size() > 4 && name.ends_with("Size"));
}

auto source_map_message(std::string_view message,
                        const diagnostics::SourceProvenanceMap& provenance) -> std::string {
    static const std::regex LINE_PATTERN(R"((?:^|[:(])(\d+)(?::\d+)?(?:[:)]))");

    std::string mapped_message(message);
    std::smatch match;
    if (!std::regex_search(mapped_message, match, LINE_PATTERN)) {
        return mapped_message;
    }

    const auto expanded_line = static_cast<uint32_t>(std::stoul(match[1].str()));
    const auto* entry = provenance.lookup(expanded_line);
    if (entry == nullptr || entry->original_file.empty() || entry->original_line == 0) {
        return mapped_message;
    }

    mapped_message += " [source: ";
    mapped_message += entry->original_file;
    mapped_message += ':';
    mapped_message += std::to_string(entry->original_line);
    if (entry->rewrite_applied && !entry->rewrite_description.empty()) {
        mapped_message += ", rewrite=";
        mapped_message += entry->rewrite_description;
    }
    mapped_message += ']';
    return mapped_message;
}

void apply_source_mapping(diagnostics::CompileReport* report,
                          const diagnostics::SourceProvenanceMap& vertex_provenance,
                          const diagnostics::SourceProvenanceMap& fragment_provenance) {
    if (report == nullptr) {
        return;
    }

    auto stages = report->stages();
    diagnostics::CompileReport mapped_report;
    for (auto stage : stages) {
        const auto& provenance = stage.stage == diagnostics::CompileStage::vertex
                                     ? vertex_provenance
                                     : fragment_provenance;
        for (auto& message : stage.messages) {
            message = source_map_message(message, provenance);
        }
        mapped_report.add_stage(std::move(stage));
    }
    *report = std::move(mapped_report);
}

void append_reflection_summary(std::string_view label, const render::ReflectionData& reflection,
                               std::vector<std::string>* summary) {
    if (reflection.ubo) {
        summary->push_back(std::string(label) +
                           ":ubo:" + std::to_string(reflection.ubo->members.size()));
    }
    if (reflection.push_constants) {
        summary->push_back(std::string(label) + ":push_constants:" +
                           std::to_string(reflection.push_constants->members.size()));
    }
    for (const auto& texture : reflection.textures) {
        summary->push_back(std::string(label) + ":texture:" + texture.name + "@" +
                           std::to_string(texture.binding));
    }
    for (const auto& input : reflection.vertex_inputs) {
        summary->push_back(std::string(label) + ":vertex_input:" + input.name + "@" +
                           std::to_string(input.location));
    }
}

void append_semantic_findings(const render::ReflectionData& reflection, size_t pass_index,
                              std::vector<std::string>* findings) {
    auto check_members = [&](const auto& members) {
        for (const auto& member : members) {
            if (!semantic_name(member.name)) {
                findings->push_back("unresolved_semantic:pass=" + std::to_string(pass_index) +
                                    ":member=" + member.name);
            }
        }
    };
    if (reflection.ubo) {
        check_members(reflection.ubo->members);
    }
    if (reflection.push_constants) {
        check_members(reflection.push_constants->members);
    }
}

auto collect_preset_paths(const ShaderBatchReportOptions& options)
    -> std::vector<std::filesystem::path> {
    if (!options.preset_paths.empty()) {
        return options.preset_paths;
    }

    std::vector<std::filesystem::path> presets;
    if (options.presets_root.empty() || !std::filesystem::exists(options.presets_root)) {
        return presets;
    }

    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(options.presets_root, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            break;
        }
        if (!it->is_regular_file(ec) || it->path().extension() != ".slangp") {
            continue;
        }
        if (options.category_filter.has_value()) {
            const auto relative = std::filesystem::relative(it->path(), options.presets_root, ec);
            if (ec) {
                continue;
            }
            const auto prefix = options.category_filter.value() + "/";
            if (!relative.generic_string().starts_with(prefix)) {
                continue;
            }
        }
        presets.push_back(it->path());
    }

    std::sort(presets.begin(), presets.end());
    return presets;
}

auto process_pass(render::ShaderRuntime& shader_runtime,
                  const render::ShaderPassConfig& pass_config, size_t pass_index)
    -> Result<BatchPassResult> {
    BatchPassResult result{};
    render::RetroArchPreprocessor preprocessor;
    diagnostics::SourceProvenanceMap provenance;
    auto preprocessed = preprocessor.preprocess(pass_config.shader_path, &provenance);
    if (!preprocessed) {
        return make_error<BatchPassResult>(preprocessed.error().code, preprocessed.error().message,
                                           preprocessed.error().location);
    }

    diagnostics::CompileReport compile_report;
    auto compile_result = shader_runtime.compile_retroarch_shader(
        preprocessed->vertex_source, preprocessed->fragment_source,
        pass_config.shader_path.stem().string(), &compile_report);
    apply_source_mapping(&compile_report, preprocessed->vertex_provenance,
                         preprocessed->fragment_provenance);
    result.compile_report = std::move(compile_report);
    if (!compile_result) {
        result.error =
            source_map_message(compile_result.error().message, preprocessed->vertex_provenance);
        return result;
    }

    result.compile_ok = true;
    for (const auto& parameter : preprocessed->parameters) {
        result.parameter_names.push_back(parameter.name);
    }
    append_reflection_summary("vertex", compile_result->vertex_reflection,
                              &result.reflection_summary);
    append_reflection_summary("fragment", compile_result->fragment_reflection,
                              &result.reflection_summary);
    append_semantic_findings(compile_result->vertex_reflection, pass_index,
                             &result.semantic_findings);
    append_semantic_findings(compile_result->fragment_reflection, pass_index,
                             &result.semantic_findings);

    return result;
}

auto process_preset(render::ShaderRuntime& shader_runtime, const std::filesystem::path& preset_path)
    -> BatchPresetResult {
    BatchPresetResult result{};
    result.path = preset_path;

    render::PresetParser parser;
    const auto preset_result = parser.load(preset_path);
    if (!preset_result) {
        result.error = preset_result.error().message;
        return result;
    }
    result.parse_ok = true;

    std::unordered_map<std::string, std::vector<size_t>> parameter_to_passes;
    std::unordered_set<std::string> reflected_parameters;
    bool degraded = false;

    for (size_t pass_index = 0; pass_index < preset_result->passes.size(); ++pass_index) {
        auto pass_result =
            process_pass(shader_runtime, preset_result->passes[pass_index], pass_index);
        if (!pass_result) {
            result.error = pass_result.error().message;
            result.authoring_verdict = "fail";
            return result;
        }

        if (!pass_result->compile_ok) {
            result.pass_results.push_back(std::move(*pass_result));
            result.error = result.pass_results.back().error;
            result.authoring_verdict = "fail";
            return result;
        }

        for (const auto& name : pass_result->parameter_names) {
            parameter_to_passes[name].push_back(pass_index);
            reflected_parameters.insert(name);
        }
        result.pass_results.push_back(std::move(*pass_result));
    }

    result.compile_ok = true;
    result.authoring_verdict = degraded ? "degraded" : "pass";

    for (const auto& [name, passes] : parameter_to_passes) {
        if (passes.size() > 1U) {
            std::ostringstream stream;
            stream << "duplicate_parameter:" << name << ":passes=";
            for (size_t i = 0; i < passes.size(); ++i) {
                if (i > 0) {
                    stream << ',';
                }
                stream << passes[i];
            }
            result.conformance_findings.push_back(stream.str());
        }
    }

    for (const auto& override_value : preset_result->parameters) {
        if (!reflected_parameters.contains(override_value.name)) {
            result.conformance_findings.push_back("unused_override:" + override_value.name);
        }
    }

    for (const auto& pass_result : result.pass_results) {
        result.conformance_findings.insert(result.conformance_findings.end(),
                                           pass_result.semantic_findings.begin(),
                                           pass_result.semantic_findings.end());
    }

    if (!result.conformance_findings.empty() && result.authoring_verdict == "pass") {
        result.authoring_verdict = "degraded";
    }

    return result;
}

auto has_reflection_loss(const BatchPassResult& pass_result) -> bool {
    const bool has_parameter = !pass_result.parameter_names.empty();
    const bool has_texture = std::any_of(
        pass_result.reflection_summary.begin(), pass_result.reflection_summary.end(),
        [](const std::string& entry) { return entry.find(":texture:") != std::string::npos; });
    return !has_parameter && !has_texture;
}

auto process_preset(render::ShaderRuntime& shader_runtime, const std::filesystem::path& preset_path,
                    bool strict_mode) -> BatchPresetResult {
    auto result = process_preset(shader_runtime, preset_path);
    if (!result.parse_ok || !result.compile_ok || result.authoring_verdict == "fail") {
        return result;
    }

    bool reflection_loss = false;
    for (size_t pass_index = 0; pass_index < result.pass_results.size(); ++pass_index) {
        if (!has_reflection_loss(result.pass_results[pass_index])) {
            continue;
        }

        reflection_loss = true;
        result.conformance_findings.push_back("reflection_loss:pass=" + std::to_string(pass_index));
    }

    if (!reflection_loss) {
        return result;
    }

    result.authoring_verdict = strict_mode ? "fail" : "degraded";
    if (strict_mode) {
        result.error = "Strict mode rejected passes with reflection loss";
    }
    return result;
}

void update_summary(const BatchPresetResult& result, BatchSummary* summary) {
    ++summary->total;
    if (!result.parse_ok || !result.compile_ok || result.authoring_verdict == "fail") {
        ++summary->failed;
    } else if (result.authoring_verdict == "degraded") {
        ++summary->degraded;
    } else {
        ++summary->passed;
    }
}

void write_json_report(const std::vector<BatchPresetResult>& results, const BatchSummary& summary,
                       const std::filesystem::path& output_path) {
    std::filesystem::create_directories(output_path.parent_path());
    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    out << "{\n  \"summary\": {\n";
    out << "    \"total\": " << summary.total << ",\n";
    out << "    \"passed\": " << summary.passed << ",\n";
    out << "    \"failed\": " << summary.failed << ",\n";
    out << "    \"skipped\": " << summary.skipped << ",\n";
    out << "    \"degraded\": " << summary.degraded << "\n";
    out << "  },\n  \"presets\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        out << "    {\n";
        out << "      \"path\": \"" << json_escape(result.path.generic_string()) << "\",\n";
        out << "      \"parse_ok\": " << (result.parse_ok ? "true" : "false") << ",\n";
        out << "      \"compile_ok\": " << (result.compile_ok ? "true" : "false") << ",\n";
        out << "      \"error\": ";
        if (result.error.has_value()) {
            out << "\"" << json_escape(*result.error) << "\"";
        } else {
            out << "null";
        }
        out << ",\n";
        out << "      \"authoring_verdict\": \"" << result.authoring_verdict << "\",\n";
        out << "      \"compile_report\": [\n";
        for (size_t pass_index = 0; pass_index < result.pass_results.size(); ++pass_index) {
            const auto& pass_result = result.pass_results[pass_index];
            out << "        {\n          \"pass_ordinal\": " << pass_index << ",\n";
            out << "          \"stages\": [\n";
            const auto& stages = pass_result.compile_report.stages();
            for (size_t stage_index = 0; stage_index < stages.size(); ++stage_index) {
                const auto& stage = stages[stage_index];
                out << "            {\n";
                out << "              \"stage\": \"" << stage_name(stage.stage) << "\",\n";
                out << "              \"success\": " << (stage.success ? "true" : "false") << ",\n";
                out << "              \"timing_us\": " << stage.timing_us << ",\n";
                out << "              \"cache_hit\": " << (stage.cache_hit ? "true" : "false")
                    << ",\n";
                out << "              \"messages\": [";
                for (size_t msg_index = 0; msg_index < stage.messages.size(); ++msg_index) {
                    if (msg_index > 0) {
                        out << ", ";
                    }
                    out << "\"" << json_escape(stage.messages[msg_index]) << "\"";
                }
                out << "]\n            }" << (stage_index + 1 < stages.size() ? "," : "") << "\n";
            }
            out << "          ]\n        }"
                << (pass_index + 1 < result.pass_results.size() ? "," : "") << "\n";
        }
        out << "      ],\n";
        out << "      \"reflection_summary\": [\n";
        for (size_t pass_index = 0; pass_index < result.pass_results.size(); ++pass_index) {
            out << "        [";
            const auto& summary_entries = result.pass_results[pass_index].reflection_summary;
            for (size_t entry_index = 0; entry_index < summary_entries.size(); ++entry_index) {
                if (entry_index > 0) {
                    out << ", ";
                }
                out << "\"" << json_escape(summary_entries[entry_index]) << "\"";
            }
            out << "]" << (pass_index + 1 < result.pass_results.size() ? "," : "") << "\n";
        }
        out << "      ],\n";
        out << "      \"conformance_findings\": [";
        for (size_t finding_index = 0; finding_index < result.conformance_findings.size();
             ++finding_index) {
            if (finding_index > 0) {
                out << ", ";
            }
            out << "\"" << json_escape(result.conformance_findings[finding_index]) << "\"";
        }
        out << "]\n";
        out << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
}

} // namespace

auto run_shader_batch_report(const ShaderBatchReportOptions& options) -> Result<int> {
    const auto preset_paths = collect_preset_paths(options);
    if (preset_paths.empty()) {
        return make_error<int>(ErrorCode::invalid_data,
                               "No shader presets matched the batch query");
    }

    const auto cache_dir = std::filesystem::temp_directory_path() / "goggles_shader_batch_cache";
    std::filesystem::create_directories(cache_dir);
    auto shader_runtime_result = render::ShaderRuntime::create(cache_dir);
    if (!shader_runtime_result) {
        return make_error<int>(shader_runtime_result.error().code,
                               shader_runtime_result.error().message,
                               shader_runtime_result.error().location);
    }
    auto shader_runtime = std::move(*shader_runtime_result);

    std::vector<BatchPresetResult> results;
    results.reserve(preset_paths.size());
    BatchSummary summary{};
    for (const auto& preset_path : preset_paths) {
        auto result = process_preset(*shader_runtime, preset_path, options.strict_mode);
        update_summary(result, &summary);
        results.push_back(std::move(result));
    }

    write_json_report(results, summary, options.output_path);
    shader_runtime->shutdown();
    return summary.failed == 0 ? 0 : 1;
}

} // namespace goggles::test
