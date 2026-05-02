#pragma once

#include <filesystem>
#include <optional>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/include_filter_options.h"

namespace xray::hexagon::ports::driving {

struct InputPathArgument {
    std::filesystem::path original_argument;
    std::filesystem::path path_for_io;
    bool was_relative{false};
};

struct AnalyzeProjectRequest {
    std::optional<InputPathArgument> compile_commands_path;
    std::optional<InputPathArgument> cmake_file_api_path;
    std::filesystem::path report_display_base;
    model::IncludeScope include_scope{model::IncludeScope::all};
    model::IncludeDepthFilter include_depth{model::IncludeDepthFilter::all};
};

class AnalyzeProjectPort {
public:
    virtual ~AnalyzeProjectPort() = default;

    virtual model::AnalysisResult analyze_project(AnalyzeProjectRequest request) const = 0;
};

}  // namespace xray::hexagon::ports::driving
