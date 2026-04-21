#include <iostream>

#include "adapters/output/placeholder_report_adapter.h"
#include "hexagon/services/startup_message_service.h"

int main() {
    const auto startup_message = xray::hexagon::services::make_startup_message();
    std::cout << xray::adapters::output::render_startup_message(startup_message) << '\n';
    return 0;
}
