#pragma once

#include <string_view>

namespace xray::hexagon::model {

struct ApplicationInfo {
    std::string_view name;
    std::string_view version;
};

constexpr ApplicationInfo application_info() {
    return {"cmake-xray", "v0.2.0"};
}

}  // namespace xray::hexagon::model
