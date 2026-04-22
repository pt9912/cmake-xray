#pragma once

#include <vector>

#include "hexagon/model/diagnostic.h"

namespace xray::hexagon::services {

void append_unique_diagnostic(std::vector<model::Diagnostic>& target,
                              const model::Diagnostic& diagnostic);
void append_unique_diagnostics(std::vector<model::Diagnostic>& target,
                               const std::vector<model::Diagnostic>& diagnostics);
void normalize_report_diagnostics(std::vector<model::Diagnostic>& diagnostics);

}  // namespace xray::hexagon::services
