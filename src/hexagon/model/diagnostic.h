#pragma once

#include <string>

namespace xray::hexagon::model {

enum class DiagnosticSeverity {
    note,
    warning,
};

struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::note};
    std::string message;
};

}  // namespace xray::hexagon::model
