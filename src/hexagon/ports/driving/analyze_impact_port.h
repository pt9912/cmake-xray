#pragma once

#include <string>
#include <string_view>

namespace xray::hexagon::ports::driving {

class AnalyzeImpactPort {
public:
    virtual ~AnalyzeImpactPort() = default;

    virtual std::string analyze_impact(std::string_view changed_path) const = 0;
};

}  // namespace xray::hexagon::ports::driving
