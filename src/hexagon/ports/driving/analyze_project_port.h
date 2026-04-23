#pragma once

#include <string_view>

#include "hexagon/model/analysis_result.h"

namespace xray::hexagon::ports::driving {

struct AnalyzeProjectRequest {
    std::string_view compile_commands_path;
    std::string_view cmake_file_api_path;
};

class AnalyzeProjectPort {
public:
    virtual ~AnalyzeProjectPort() = default;

    virtual model::AnalysisResult analyze_project(AnalyzeProjectRequest request) const = 0;
};

}  // namespace xray::hexagon::ports::driving
