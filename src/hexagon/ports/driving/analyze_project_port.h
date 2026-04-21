#pragma once

#include "hexagon/model/analysis_result.h"

namespace xray::hexagon::ports::driving {

class AnalyzeProjectPort {
public:
    virtual ~AnalyzeProjectPort() = default;

    virtual model::AnalysisResult analyze_project() const = 0;
};

}  // namespace xray::hexagon::ports::driving
