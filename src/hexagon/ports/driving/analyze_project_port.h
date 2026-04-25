#pragma once

#include <filesystem>
#include <optional>

#include "hexagon/model/analysis_result.h"

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
};

class AnalyzeProjectPort {
public:
    virtual ~AnalyzeProjectPort() = default;

    virtual model::AnalysisResult analyze_project(AnalyzeProjectRequest request) const = 0;
};

}  // namespace xray::hexagon::ports::driving
