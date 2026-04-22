#pragma once

#include <ostream>

#include "hexagon/ports/driving/analyze_impact_port.h"
#include "hexagon/ports/driving/generate_report_port.h"
#include "hexagon/ports/driving/analyze_project_port.h"

namespace xray::adapters::cli {

class CliAdapter {
public:
    CliAdapter(const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port,
               const xray::hexagon::ports::driving::AnalyzeImpactPort& analyze_impact_port,
               const xray::hexagon::ports::driving::GenerateReportPort& generate_report_port);

    int run(int argc, const char* const* argv, std::ostream& out, std::ostream& err) const;

private:
    const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port_;
    const xray::hexagon::ports::driving::AnalyzeImpactPort& analyze_impact_port_;
    const xray::hexagon::ports::driving::GenerateReportPort& generate_report_port_;
};

}  // namespace xray::adapters::cli
