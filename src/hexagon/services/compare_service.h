#pragma once

#include "hexagon/ports/driven/analysis_report_reader_port.h"
#include "hexagon/ports/driving/compare_analysis_port.h"

namespace xray::hexagon::services {

class CompareService final : public ports::driving::CompareAnalysisPort {
public:
    explicit CompareService(const ports::driven::AnalysisReportReaderPort& reader);

    model::CompareResult compare(
        ports::driving::CompareAnalysisRequest request) const override;

private:
    const ports::driven::AnalysisReportReaderPort& reader_;
};

}  // namespace xray::hexagon::services
