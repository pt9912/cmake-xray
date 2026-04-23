#pragma once

#include <string>
#include <vector>

namespace xray::hexagon::model {

struct TargetInfo {
    std::string display_name;
    std::string type;
    std::string unique_key;
};

struct TargetAssignment {
    std::string observation_key;
    std::vector<TargetInfo> targets;
};

}  // namespace xray::hexagon::model
