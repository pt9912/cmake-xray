#pragma once

#include "hexagon/ports/driven/report_writer_port.h"

namespace xray::adapters::output {

class PlaceholderReportAdapter final : public xray::hexagon::ports::driven::ReportWriterPort {
public:
    std::string write_report(const xray::hexagon::model::AnalysisResult& analysis_result) const override;
};

}  // namespace xray::adapters::output
