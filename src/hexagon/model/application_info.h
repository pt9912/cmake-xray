#pragma once

#include <string_view>

#ifndef XRAY_APP_VERSION_STRING
#error "XRAY_APP_VERSION_STRING must be defined by the build system. See plan-M5-1-6.md and the XRAY_APP_VERSION/XRAY_VERSION_SUFFIX cache variables in the root CMakeLists.txt."
#endif

namespace xray::hexagon::model {

struct ApplicationInfo {
    std::string_view name;
    std::string_view version;
};

constexpr ApplicationInfo application_info() {
    // AP M5-1.6 Tranche A: the version is the canonical published app version
    // without the leading 'v'; the CI release job sets XRAY_APP_VERSION from
    // the validated tag, local builds fall back to PROJECT_VERSION (plus an
    // optional XRAY_VERSION_SUFFIX).
    return {"cmake-xray", XRAY_APP_VERSION_STRING};
}

}  // namespace xray::hexagon::model
