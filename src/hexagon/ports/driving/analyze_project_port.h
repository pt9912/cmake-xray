#pragma once

#include <string_view>

#include "hexagon/model/analysis_result.h"

namespace xray::hexagon::ports::driving {

class AnalyzeProjectPort {
public:
    virtual ~AnalyzeProjectPort() = default;

    virtual model::AnalysisResult analyze_project(std::string_view compile_commands_path) const = 0;
};

}  // namespace xray::hexagon::ports::driving
