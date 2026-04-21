#pragma once

#include <string>

#include "hexagon/model/analysis_result.h"

namespace xray::hexagon::ports::driving {

class GenerateReportPort {
public:
    virtual ~GenerateReportPort() = default;

    virtual std::string generate_report(const model::AnalysisResult& analysis_result) const = 0;
};

}  // namespace xray::hexagon::ports::driving
