#pragma once

#include "hexagon/ports/driven/report_writer_port.h"
#include "hexagon/ports/driving/generate_report_port.h"

namespace xray::hexagon::services {

class ReportGenerator final : public ports::driving::GenerateReportPort {
public:
    explicit ReportGenerator(const ports::driven::ReportWriterPort& report_writer_port);

    std::string generate_report(const model::AnalysisResult& analysis_result) const override;

private:
    const ports::driven::ReportWriterPort& report_writer_port_;
};

}  // namespace xray::hexagon::services
