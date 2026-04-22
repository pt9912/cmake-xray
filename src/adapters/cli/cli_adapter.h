#pragma once

#include <ostream>

#include "hexagon/ports/driving/analyze_impact_port.h"
#include "hexagon/ports/driving/generate_report_port.h"
#include "hexagon/ports/driving/analyze_project_port.h"

namespace xray::adapters::cli {

struct ReportPorts {
    const xray::hexagon::ports::driving::GenerateReportPort& console;
    const xray::hexagon::ports::driving::GenerateReportPort& markdown;
};

class CliAdapter {
public:
    CliAdapter(const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port,
               const xray::hexagon::ports::driving::AnalyzeImpactPort& analyze_impact_port,
               ReportPorts report_ports);

    int run(int argc, const char* const* argv, std::ostream& out, std::ostream& err) const;

private:
    const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port_;
    const xray::hexagon::ports::driving::AnalyzeImpactPort& analyze_impact_port_;
    ReportPorts report_ports_;
};

}  // namespace xray::adapters::cli
