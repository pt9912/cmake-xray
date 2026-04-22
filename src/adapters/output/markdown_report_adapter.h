#pragma once

#include "hexagon/ports/driven/report_writer_port.h"

namespace xray::adapters::output {

class MarkdownReportAdapter final : public xray::hexagon::ports::driven::ReportWriterPort {
public:
    std::string write_analysis_report(
        const xray::hexagon::model::AnalysisResult& analysis_result,
        std::size_t top_limit) const override;
    std::string write_impact_report(
        const xray::hexagon::model::ImpactResult& impact_result) const override;
};

}  // namespace xray::adapters::output
