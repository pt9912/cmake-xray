#pragma once

#include <cstddef>
#include <string>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/impact_result.h"

namespace xray::hexagon::ports::driving {

class GenerateReportPort {
public:
    virtual ~GenerateReportPort() = default;

    virtual std::string generate_analysis_report(const model::AnalysisResult& analysis_result,
                                                 std::size_t top_limit) const = 0;
    virtual std::string generate_impact_report(const model::ImpactResult& impact_result) const = 0;
};

}  // namespace xray::hexagon::ports::driving
