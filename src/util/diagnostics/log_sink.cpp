#include "log_sink.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace goggles::diagnostics {

namespace {

auto severity_to_spdlog(Severity severity) -> spdlog::level::level_enum {
    switch (severity) {
    case Severity::debug:
        return spdlog::level::debug;
    case Severity::info:
        return spdlog::level::info;
    case Severity::warning:
        return spdlog::level::warn;
    case Severity::error:
        return spdlog::level::err;
    }
    return spdlog::level::info;
}

auto category_name(Category category) -> const char* {
    switch (category) {
    case Category::authoring:
        return "authoring";
    case Category::runtime:
        return "runtime";
    case Category::quality:
        return "quality";
    case Category::capture:
        return "capture";
    }
    return "unknown";
}

} // namespace

LogSink::LogSink() {
    m_logger = spdlog::get("diagnostics");
    if (!m_logger) {
        m_logger = spdlog::stderr_color_mt("diagnostics");
    }
}

void LogSink::receive(const DiagnosticEvent& event) {
    if (!m_logger) {
        return;
    }
    auto level = severity_to_spdlog(event.severity);
    if (event.localization.pass_ordinal == LocalizationKey::CHAIN_LEVEL) {
        m_logger->log(level, "[{}] [chain] {}", category_name(event.category), event.message);
    } else {
        m_logger->log(level, "[{}] [pass {}] {}", category_name(event.category),
                      event.localization.pass_ordinal, event.message);
    }
}

} // namespace goggles::diagnostics
