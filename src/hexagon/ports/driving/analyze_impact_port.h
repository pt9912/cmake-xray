#pragma once

#include <filesystem>
#include <string_view>

#include "hexagon/model/impact_result.h"

namespace xray::hexagon::ports::driving {

class AnalyzeImpactPort {
public:
    virtual ~AnalyzeImpactPort() = default;

    virtual model::ImpactResult analyze_impact(std::string_view compile_commands_path,
                                               const std::filesystem::path& changed_path) const = 0;
};

}  // namespace xray::hexagon::ports::driving
