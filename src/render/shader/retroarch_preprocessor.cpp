#include "retroarch_preprocessor.hpp"

#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <util/logging.hpp>
#include <util/profiling.hpp>

namespace goggles::render {

namespace {

constexpr std::string_view PRAGMA_STAGE_VERTEX = "#pragma stage vertex";
constexpr std::string_view PRAGMA_STAGE_FRAGMENT = "#pragma stage fragment";

struct FilterResult {
    std::string source;
    diagnostics::SourceProvenanceMap provenance;
};

struct StageSplitResult {
    std::string vertex_source;
    std::string fragment_source;
    diagnostics::SourceProvenanceMap vertex_provenance;
    diagnostics::SourceProvenanceMap fragment_provenance;
};

auto read_file(const std::filesystem::path& path) -> Result<std::string> {
    std::ifstream file(path);
    if (!file) {
        return make_error<std::string>(ErrorCode::file_not_found,
                                       "Failed to open file: " + path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

auto trim(const std::string& str) -> std::string {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Slang doesn't support vec *= mat compound assignment
auto fix_compound_assign(const std::string& source) -> std::string {
    static const std::regex COMPOUND_ASSIGN(R"((\b\w+(?:\.\w+)?)\s*\*=\s*([^;]+);)");
    std::smatch match;
    std::string::const_iterator search_start = source.cbegin();
    std::string output;
    size_t last_pos = 0;

    auto is_matrix_expr = [](const std::string& expr) {
        return expr.find("mat") != std::string::npos ||
               expr.find("transpose") != std::string::npos ||
               expr.find("inverse") != std::string::npos || expr.find("IPT") != std::string::npos ||
               expr.find("LMS") != std::string::npos || expr.find("CAT") != std::string::npos ||
               expr.find("RGB") != std::string::npos || expr.find("XYZ") != std::string::npos ||
               expr.find("YUV") != std::string::npos || expr.find("color") != std::string::npos;
    };

    while (std::regex_search(search_start, source.cend(), match, COMPOUND_ASSIGN)) {
        std::string var = match[1].str();
        std::string expr = match[2].str();
        if (is_matrix_expr(expr)) {
            auto match_pos =
                static_cast<size_t>(match.position() + (search_start - source.cbegin()));
            output += source.substr(last_pos, match_pos - last_pos);
            output.append(var).append(" = ").append(var).append(" * (").append(expr).append(");");
            last_pos = match_pos + static_cast<size_t>(match.length());
        }
        search_start = match.suffix().first;
    }
    output += source.substr(last_pos);
    return output;
}

// Slang doesn't support mat3==mat3 in ternary (returns bmat3 instead of bool)
auto fix_matrix_compare(const std::string& source) -> std::string {
    // Replace (m_in==m_ou) with (m_in[0]==m_ou[0] && m_in[1]==m_ou[1] && m_in[2]==m_ou[2])
    static const std::regex MAT_COMPARE(R"(\((\w+)\s*==\s*(\w+)\))");
    std::smatch match;

    auto is_matrix_var = [](const std::string& name) {
        return name.starts_with("m_") || name.find("_mat") != std::string::npos ||
               name.find("prims") != std::string::npos;
    };

    std::string::const_iterator search_start = source.cbegin();
    std::string output;
    size_t last_pos = 0;

    while (std::regex_search(search_start, source.cend(), match, MAT_COMPARE)) {
        std::string lhs = match[1].str();
        std::string rhs = match[2].str();
        if (is_matrix_var(lhs) && is_matrix_var(rhs)) {
            auto match_pos =
                static_cast<size_t>(match.position() + (search_start - source.cbegin()));
            output += source.substr(last_pos, match_pos - last_pos);
            output.append("(").append(lhs).append("[0]==").append(rhs).append("[0] && ");
            output.append(lhs).append("[1]==").append(rhs).append("[1] && ");
            output.append(lhs).append("[2]==").append(rhs).append("[2])");
            last_pos = match_pos + static_cast<size_t>(match.length());
        }
        search_start = match.suffix().first;
    }
    output += source.substr(last_pos);
    return output;
}

auto fix_slang_compat(const std::string& source) -> std::string {
    std::string result = fix_compound_assign(source);
    result = fix_matrix_compare(result);
    return result;
}

auto apply_compat_rewrites(std::string resolved, diagnostics::SourceProvenanceMap* provenance)
    -> std::string {
    std::string before_compat = resolved;
    resolved = fix_slang_compat(resolved);

    if (provenance != nullptr && resolved != before_compat) {
        uint32_t line_num = 0;
        std::istringstream old_stream(before_compat);
        std::istringstream new_stream(resolved);
        std::string old_line;
        std::string new_line;
        while (std::getline(old_stream, old_line) && std::getline(new_stream, new_line)) {
            ++line_num;
            if (old_line != new_line) {
                auto* existing = provenance->lookup(line_num);
                if (existing != nullptr) {
                    diagnostics::ProvenanceEntry updated = *existing;
                    updated.rewrite_applied = true;
                    updated.rewrite_description = "Slang compatibility fix";
                    provenance->record(line_num, std::move(updated));
                } else {
                    provenance->record(line_num,
                                       {.original_file = {},
                                        .original_line = 0,
                                        .rewrite_applied = true,
                                        .rewrite_description = "Slang compatibility fix"});
                }
            }
        }
    }

    return resolved;
}

template <typename Predicate>
auto filter_lines_with_provenance(const std::string& source,
                                  const diagnostics::SourceProvenanceMap* provenance,
                                  Predicate&& keep_line) -> FilterResult {
    FilterResult result;
    std::istringstream stream(source);
    std::string line;
    uint32_t input_line = 0;
    uint32_t output_line = 0;

    while (std::getline(stream, line)) {
        ++input_line;
        if (!keep_line(line)) {
            continue;
        }

        ++output_line;
        result.source += line;
        result.source += "\n";

        if (provenance != nullptr) {
            if (const auto* entry = provenance->lookup(input_line); entry != nullptr) {
                result.provenance.record(output_line, *entry);
            }
        }
    }

    return result;
}

auto split_by_stage_with_provenance(const std::string& source,
                                    const diagnostics::SourceProvenanceMap* provenance)
    -> StageSplitResult {
    enum class Stage : std::uint8_t { shared, vertex, fragment };

    StageSplitResult result;
    Stage current_stage = Stage::shared;
    bool saw_vertex_stage = false;
    bool saw_fragment_stage = false;
    uint32_t vertex_line = 0;
    uint32_t fragment_line = 0;

    auto append_line = [](std::string* target_source,
                          diagnostics::SourceProvenanceMap* target_provenance,
                          uint32_t* target_line, const std::string& line,
                          const std::optional<diagnostics::ProvenanceEntry>& entry) {
        ++(*target_line);
        *target_source += line;
        *target_source += "\n";
        if (entry.has_value()) {
            target_provenance->record(*target_line, *entry);
        }
    };

    std::vector<std::string> shared_lines;
    std::vector<std::optional<diagnostics::ProvenanceEntry>> shared_entries;

    std::istringstream stream(source);
    std::string line;
    uint32_t input_line = 0;
    while (std::getline(stream, line)) {
        ++input_line;
        const std::string trimmed = trim(line);
        const std::optional<diagnostics::ProvenanceEntry> entry =
            provenance != nullptr && provenance->lookup(input_line) != nullptr
                ? std::optional<diagnostics::ProvenanceEntry>{*provenance->lookup(input_line)}
                : std::nullopt;

        if (trimmed.starts_with(PRAGMA_STAGE_VERTEX)) {
            if (!saw_vertex_stage) {
                for (size_t index = 0; index < shared_lines.size(); ++index) {
                    append_line(&result.vertex_source, &result.vertex_provenance, &vertex_line,
                                shared_lines[index], shared_entries[index]);
                }
            }
            saw_vertex_stage = true;
            current_stage = Stage::vertex;
            continue;
        }

        if (trimmed.starts_with(PRAGMA_STAGE_FRAGMENT)) {
            if (!saw_fragment_stage) {
                for (size_t index = 0; index < shared_lines.size(); ++index) {
                    append_line(&result.fragment_source, &result.fragment_provenance,
                                &fragment_line, shared_lines[index], shared_entries[index]);
                }
            }
            saw_fragment_stage = true;
            current_stage = Stage::fragment;
            continue;
        }

        if (current_stage == Stage::shared) {
            shared_lines.push_back(line);
            shared_entries.push_back(entry);
            continue;
        }

        if (current_stage == Stage::vertex) {
            append_line(&result.vertex_source, &result.vertex_provenance, &vertex_line, line,
                        entry);
        } else {
            append_line(&result.fragment_source, &result.fragment_provenance, &fragment_line, line,
                        entry);
        }
    }

    if (!saw_vertex_stage && !saw_fragment_stage) {
        result.vertex_source = source;
        result.fragment_source = source;
        if (provenance != nullptr) {
            result.vertex_provenance = *provenance;
            result.fragment_provenance = *provenance;
        }
    } else {
        if (!saw_vertex_stage) {
            result.vertex_source = result.fragment_source;
            result.vertex_provenance = result.fragment_provenance;
        }
        if (!saw_fragment_stage) {
            result.fragment_source = result.vertex_source;
            result.fragment_provenance = result.vertex_provenance;
        }
    }

    return result;
}

} // namespace

auto RetroArchPreprocessor::preprocess(const std::filesystem::path& shader_path,
                                       diagnostics::SourceProvenanceMap* provenance)
    -> Result<PreprocessedShader> {
    GOGGLES_PROFILE_FUNCTION();
    auto source_result = read_file(shader_path);
    if (!source_result) {
        return make_error<PreprocessedShader>(source_result.error().code,
                                              source_result.error().message);
    }

    auto resolved_result =
        resolve_includes(source_result.value(), shader_path.parent_path(), shader_path, provenance);
    if (!resolved_result) {
        return make_error<PreprocessedShader>(resolved_result.error().code,
                                              resolved_result.error().message);
    }

    std::string resolved = apply_compat_rewrites(std::move(resolved_result.value()), provenance);

    std::vector<ShaderParameter> parameters;
    static const std::regex PARAM_REGEX(
        R"regex(^\s*#pragma\s+parameter\s+(\w+)\s+"([^"]+)"\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+))regex");
    const auto parameter_filter =
        filter_lines_with_provenance(resolved, provenance, [&](const std::string& line) {
            std::smatch match;
            if (std::regex_search(line, match, PARAM_REGEX)) {
                ShaderParameter param;
                param.name = match[1].str();
                param.description = match[2].str();
                param.default_value = std::stof(match[3].str());
                param.current_value = param.default_value;
                param.min_value = std::stof(match[4].str());
                param.max_value = std::stof(match[5].str());
                param.step = std::stof(match[6].str());
                parameters.push_back(std::move(param));
                return false;
            }
            return true;
        });

    ShaderMetadata metadata;
    static const std::regex NAME_REGEX(R"(^\s*#pragma\s+name\s+(\S+))");
    static const std::regex FORMAT_REGEX(R"(^\s*#pragma\s+format\s+(\S+))");
    const auto metadata_filter = filter_lines_with_provenance(
        parameter_filter.source, &parameter_filter.provenance, [&](const std::string& line) {
            std::smatch match;
            if (std::regex_search(line, match, NAME_REGEX)) {
                metadata.name_alias = match[1].str();
                return false;
            }
            if (std::regex_search(line, match, FORMAT_REGEX)) {
                metadata.format = match[1].str();
                return false;
            }
            return true;
        });
    auto stage_split =
        split_by_stage_with_provenance(metadata_filter.source, &metadata_filter.provenance);

    if (provenance != nullptr) {
        *provenance = metadata_filter.provenance;
    }

    return PreprocessedShader{
        .vertex_source = std::move(stage_split.vertex_source),
        .fragment_source = std::move(stage_split.fragment_source),
        .vertex_provenance = std::move(stage_split.vertex_provenance),
        .fragment_provenance = std::move(stage_split.fragment_provenance),
        .parameters = std::move(parameters),
        .metadata = std::move(metadata),
    };
}

auto RetroArchPreprocessor::preprocess_source(const std::string& source,
                                              const std::filesystem::path& base_path,
                                              diagnostics::SourceProvenanceMap* provenance)
    -> Result<PreprocessedShader> {
    GOGGLES_PROFILE_FUNCTION();
    // Step 1: Resolve includes
    auto resolved_result = resolve_includes(source, base_path, {}, provenance);
    if (!resolved_result) {
        return make_error<PreprocessedShader>(resolved_result.error().code,
                                              resolved_result.error().message);
    }
    std::string resolved = apply_compat_rewrites(std::move(resolved_result.value()), provenance);

    std::vector<ShaderParameter> parameters;
    static const std::regex PARAM_REGEX(
        R"regex(^\s*#pragma\s+parameter\s+(\w+)\s+"([^"]+)"\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+))regex");
    const auto parameter_filter =
        filter_lines_with_provenance(resolved, provenance, [&](const std::string& line) {
            std::smatch match;
            if (std::regex_search(line, match, PARAM_REGEX)) {
                ShaderParameter param;
                param.name = match[1].str();
                param.description = match[2].str();
                param.default_value = std::stof(match[3].str());
                param.current_value = param.default_value;
                param.min_value = std::stof(match[4].str());
                param.max_value = std::stof(match[5].str());
                param.step = std::stof(match[6].str());
                parameters.push_back(std::move(param));
                return false;
            }
            return true;
        });

    ShaderMetadata metadata;
    static const std::regex NAME_REGEX(R"(^\s*#pragma\s+name\s+(\S+))");
    static const std::regex FORMAT_REGEX(R"(^\s*#pragma\s+format\s+(\S+))");
    const auto metadata_filter = filter_lines_with_provenance(
        parameter_filter.source, &parameter_filter.provenance, [&](const std::string& line) {
            std::smatch match;
            if (std::regex_search(line, match, NAME_REGEX)) {
                metadata.name_alias = match[1].str();
                return false;
            }
            if (std::regex_search(line, match, FORMAT_REGEX)) {
                metadata.format = match[1].str();
                return false;
            }
            return true;
        });
    auto stage_split =
        split_by_stage_with_provenance(metadata_filter.source, &metadata_filter.provenance);

    if (provenance != nullptr) {
        *provenance = metadata_filter.provenance;
    }

    return PreprocessedShader{
        .vertex_source = std::move(stage_split.vertex_source),
        .fragment_source = std::move(stage_split.fragment_source),
        .vertex_provenance = std::move(stage_split.vertex_provenance),
        .fragment_provenance = std::move(stage_split.fragment_provenance),
        .parameters = std::move(parameters),
        .metadata = std::move(metadata),
    };
}

auto RetroArchPreprocessor::resolve_includes(
    const std::string& source,
    const std::filesystem::path& base_path, // NOLINT(bugprone-easily-swappable-parameters)
    const std::filesystem::path& current_file, diagnostics::SourceProvenanceMap* provenance,
    int depth) -> Result<std::string> {
    GOGGLES_PROFILE_FUNCTION();
    if (depth > MAX_INCLUDE_DEPTH) {
        return make_error<std::string>(ErrorCode::parse_error,
                                       "Maximum include depth exceeded (circular include?)");
    }

    // Match #include "path" or #include <path>
    std::regex include_regex(R"(^\s*#include\s*["<]([^">]+)[">])");
    std::string result;
    std::istringstream stream(source);
    std::string line;
    uint32_t source_line = 0;

    while (std::getline(stream, line)) {
        ++source_line;
        std::smatch match;
        if (std::regex_search(line, match, include_regex)) {
            std::string include_path_str = match[1].str();
            std::filesystem::path include_path = base_path / include_path_str;

            auto include_source = read_file(include_path);
            if (!include_source) {
                return make_error<std::string>(ErrorCode::file_not_found,
                                               "Failed to resolve include: " +
                                                   include_path.string());
            }

            // Recursively resolve includes in the included file
            auto resolved = resolve_includes(include_source.value(), include_path.parent_path(),
                                             include_path, provenance, depth + 1);
            if (!resolved) {
                return resolved;
            }

            // Record provenance for lines from the included file
            if (provenance != nullptr) {
                uint32_t expanded_line_start =
                    static_cast<uint32_t>(std::count(result.begin(), result.end(), '\n')) + 1;
                std::istringstream inc_stream(resolved.value());
                std::string inc_line;
                uint32_t inc_line_num = 0;
                while (std::getline(inc_stream, inc_line)) {
                    ++inc_line_num;
                    provenance->record(expanded_line_start + inc_line_num - 1,
                                       {.original_file = include_path.string(),
                                        .original_line = inc_line_num,
                                        .rewrite_applied = false,
                                        .rewrite_description = {}});
                }
            }

            result += resolved.value();
            result += "\n";
        } else {
            // Record provenance for lines from the current file
            if (provenance != nullptr && !current_file.empty()) {
                uint32_t expanded_line =
                    static_cast<uint32_t>(std::count(result.begin(), result.end(), '\n')) + 1;
                provenance->record(expanded_line, {.original_file = current_file.string(),
                                                   .original_line = source_line,
                                                   .rewrite_applied = false,
                                                   .rewrite_description = {}});
            }

            result += line;
            result += "\n";
        }
    }

    return result;
}

auto RetroArchPreprocessor::split_by_stage(const std::string& source)
    -> std::pair<std::string, std::string> {
    GOGGLES_PROFILE_FUNCTION();
    enum class Stage : std::uint8_t { shared, vertex, fragment };

    std::string shared;
    std::string vertex;
    std::string fragment;
    std::string* current = &shared;
    Stage current_stage = Stage::shared;

    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);

        if (trimmed.starts_with(PRAGMA_STAGE_VERTEX)) {
            if (current_stage != Stage::vertex) {
                current = &vertex;
                vertex = shared;
                current_stage = Stage::vertex;
            }
            continue;
        }

        if (trimmed.starts_with(PRAGMA_STAGE_FRAGMENT)) {
            if (current_stage != Stage::fragment) {
                current = &fragment;
                fragment = shared;
                current_stage = Stage::fragment;
            }
            continue;
        }

        *current += line;
        *current += "\n";
    }

    if (vertex.empty() && fragment.empty()) {
        vertex = source;
        fragment = source;
    }

    return {vertex, fragment};
}

auto RetroArchPreprocessor::extract_parameters(const std::string& source)
    -> std::pair<std::string, std::vector<ShaderParameter>> {
    GOGGLES_PROFILE_FUNCTION();
    std::vector<ShaderParameter> parameters;
    std::string result;

    // #pragma parameter NAME "Description" default min max step
    // Using custom delimiter to handle quotes in the pattern
    std::regex param_regex(
        R"regex(^\s*#pragma\s+parameter\s+(\w+)\s+"([^"]+)"\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+))regex");

    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, param_regex)) {
            ShaderParameter param;
            param.name = match[1].str();
            param.description = match[2].str();
            param.default_value = std::stof(match[3].str());
            param.current_value = param.default_value;
            param.min_value = std::stof(match[4].str());
            param.max_value = std::stof(match[5].str());
            param.step = std::stof(match[6].str());
            parameters.push_back(param);
            // Don't add the pragma line to output
        } else {
            result += line;
            result += "\n";
        }
    }

    return {result, parameters};
}

auto RetroArchPreprocessor::extract_metadata(const std::string& source)
    -> std::pair<std::string, ShaderMetadata> {
    GOGGLES_PROFILE_FUNCTION();
    ShaderMetadata metadata;
    std::string result;

    std::regex name_regex(R"(^\s*#pragma\s+name\s+(\S+))");
    std::regex format_regex(R"(^\s*#pragma\s+format\s+(\S+))");

    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, name_regex)) {
            metadata.name_alias = match[1].str();
            // Don't add the pragma line to output
        } else if (std::regex_search(line, match, format_regex)) {
            metadata.format = match[1].str(); // NOLINT(bugprone-branch-clone)
            // Don't add the pragma line to output
        } else {
            result += line;
            result += "\n";
        }
    }

    return {result, metadata};
}

} // namespace goggles::render
