#pragma once

#include "diagnostic_event.hpp"

namespace goggles::diagnostics {

class DiagnosticSink {
public:
    virtual ~DiagnosticSink() = default;
    virtual void receive(const DiagnosticEvent& event) = 0;
};

} // namespace goggles::diagnostics
