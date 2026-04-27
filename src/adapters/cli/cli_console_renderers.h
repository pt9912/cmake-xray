#pragma once

#include <cstddef>
#include <string>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/impact_result.h"

namespace xray::adapters::cli {

// AP M5-1.5 Tranche B: CLI-owned console emitters for --format console
// --quiet and --format console --verbose. The functions take ergebnisobjekte
// and return std::string with exactly one trailing newline; they never see
// CLI11 state. Mutual-exclusion of --quiet and --verbose is enforced before
// these emitters run.
//
// Tranche B.1 ships render_console_quiet_*; Tranche B.2 will add
// render_console_verbose_analyze and render_console_verbose_impact in the
// same translation unit. Per-section helpers (`render_verbose_inputs_section`,
// `render_verbose_observation_section`, ...) live file-local in the .cpp.

std::string render_console_quiet_analyze(const xray::hexagon::model::AnalysisResult& result,
                                          std::size_t top_limit);

std::string render_console_quiet_impact(const xray::hexagon::model::ImpactResult& result);

std::string render_console_verbose_analyze(const xray::hexagon::model::AnalysisResult& result,
                                            std::size_t top_limit);

std::string render_console_verbose_impact(const xray::hexagon::model::ImpactResult& result);

}  // namespace xray::adapters::cli
