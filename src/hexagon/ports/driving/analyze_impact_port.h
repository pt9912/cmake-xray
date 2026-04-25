#pragma once

#include <filesystem>
#include <optional>

#include "hexagon/model/impact_result.h"
#include "hexagon/ports/driving/analyze_project_port.h"

namespace xray::hexagon::ports::driving {

struct AnalyzeImpactRequest {
    std::optional<InputPathArgument> compile_commands_path;
    InputPathArgument changed_file_path;
    std::optional<InputPathArgument> cmake_file_api_path;
    std::filesystem::path report_display_base;
};

class AnalyzeImpactPort {
public:
    virtual ~AnalyzeImpactPort() = default;

    virtual model::ImpactResult analyze_impact(AnalyzeImpactRequest request) const = 0;
};

}  // namespace xray::hexagon::ports::driving
