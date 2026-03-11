#pragma once

#include "diagnostic_sink.hpp"

#include <memory>
#include <spdlog/spdlog.h>

namespace goggles::diagnostics {

/// @brief Routes diagnostic events to a dedicated spdlog logger.
class LogSink : public DiagnosticSink {
public:
    LogSink();
    void receive(const DiagnosticEvent& event) override;

private:
    std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace goggles::diagnostics
