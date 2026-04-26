#pragma once

#include <cstddef>
#include <ostream>
#include <string_view>

#include "adapters/cli/atomic_report_writer.h"
#include "adapters/cli/cli_report_renderer.h"
#include "hexagon/ports/driving/analyze_impact_port.h"
#include "hexagon/ports/driving/generate_report_port.h"
#include "hexagon/ports/driving/analyze_project_port.h"

namespace xray::adapters::cli {

struct ReportPorts {
    const xray::hexagon::ports::driving::GenerateReportPort& console;
    const xray::hexagon::ports::driving::GenerateReportPort& markdown;
    const xray::hexagon::ports::driving::GenerateReportPort& json;
    const xray::hexagon::ports::driving::GenerateReportPort& dot;
    const xray::hexagon::ports::driving::GenerateReportPort& html;
};

struct CliOutputStreams {
    std::ostream& out;
    std::ostream& err;
};

class AnalysisCliReportRenderer final : public CliReportRenderer {
public:
    AnalysisCliReportRenderer(
        const xray::hexagon::ports::driving::GenerateReportPort& port,
        const xray::hexagon::model::AnalysisResult& result, std::size_t top_limit);

    RenderResult render() const override;

private:
    const xray::hexagon::ports::driving::GenerateReportPort* port_;
    const xray::hexagon::model::AnalysisResult* result_;
    std::size_t top_limit_;
};

class ImpactCliReportRenderer final : public CliReportRenderer {
public:
    ImpactCliReportRenderer(
        const xray::hexagon::ports::driving::GenerateReportPort& port,
        const xray::hexagon::model::ImpactResult& result);

    RenderResult render() const override;

private:
    const xray::hexagon::ports::driving::GenerateReportPort* port_;
    const xray::hexagon::model::ImpactResult* result_;
};

// Exposed so render-error tests can inject a CliReportRenderer doppelgaenger
// directly into the CLI write path without going through argv.
int emit_rendered_report(const CliReportRenderer& renderer,
                         std::string_view output_path,
                         AtomicReportWriter& writer,
                         CliOutputStreams streams);

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
