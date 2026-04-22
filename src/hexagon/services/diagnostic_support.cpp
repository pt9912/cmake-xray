#include "services/diagnostic_support.h"

#include <algorithm>

namespace xray::hexagon::services {

namespace {

using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;

bool diagnostics_equal(const Diagnostic& lhs, const Diagnostic& rhs) {
    return lhs.severity == rhs.severity && lhs.message == rhs.message;
}

int severity_order(DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::warning ? 0 : 1;
}

bool report_diagnostic_less(const Diagnostic& lhs, const Diagnostic& rhs) {
    if (lhs.severity != rhs.severity) {
        return severity_order(lhs.severity) < severity_order(rhs.severity);
    }

    return lhs.message < rhs.message;
}

}  // namespace

void append_unique_diagnostic(std::vector<Diagnostic>& target, const Diagnostic& diagnostic) {
    const auto duplicate = std::any_of(target.begin(), target.end(), [&](const auto& existing) {
        return diagnostics_equal(existing, diagnostic);
    });
    if (!duplicate) target.push_back(diagnostic);
}

void append_unique_diagnostics(std::vector<Diagnostic>& target,
                               const std::vector<Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        append_unique_diagnostic(target, diagnostic);
    }
}

void normalize_report_diagnostics(std::vector<Diagnostic>& diagnostics) {
    std::sort(diagnostics.begin(), diagnostics.end(), report_diagnostic_less);
    diagnostics.erase(std::unique(diagnostics.begin(), diagnostics.end(), diagnostics_equal),
                      diagnostics.end());
}

}  // namespace xray::hexagon::services
