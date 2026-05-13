#include <iostream>
#include <string_view>

#ifdef XRAY_APP_VERSION_STRING
#undef XRAY_APP_VERSION_STRING
#endif
#define XRAY_APP_VERSION_STRING "1.3.0-rc.1"

#include "hexagon/model/application_info.h"

int main() {
    constexpr std::string_view expected{"1.3.0-rc.1"};
    const auto actual = xray::hexagon::model::application_info().version;
    if (actual != expected) {
        std::cerr << "expected prerelease version " << expected << ", got " << actual << '\n';
        return 1;
    }
    if (actual.starts_with("v")) {
        std::cerr << "prerelease app version must not include a leading v\n";
        return 1;
    }
    return 0;
}
