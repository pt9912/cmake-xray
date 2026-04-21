#include "hexagon/services/startup_message_service.h"

#include "hexagon/model/application_info.h"

namespace xray::hexagon::services {

std::string make_startup_message() {
    const auto info = xray::hexagon::model::application_info();
    return std::string(info.name) + " " + std::string(info.version) +
           " placeholder binary for milestone M0";
}

}  // namespace xray::hexagon::services
