#pragma once

#include <string>

#include "hexagon/ports/driving/analyze_project_port.h"
#include "hexagon/ports/driving/generate_report_port.h"

namespace xray::adapters::cli {

class PlaceholderCliAdapter {
public:
    PlaceholderCliAdapter(
        const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port,
        const xray::hexagon::ports::driving::GenerateReportPort& generate_report_port);

    std::string description() const;
    std::string run() const;

private:
    const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port_;
    const xray::hexagon::ports::driving::GenerateReportPort& generate_report_port_;
};

}  // namespace xray::adapters::cli
