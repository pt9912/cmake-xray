#include <doctest/doctest.h>

#include "hexagon/services/startup_message_service.h"

TEST_CASE("startup message mentions the milestone placeholder") {
    const auto message = xray::hexagon::services::make_startup_message();

    CHECK(message.find("cmake-xray") != std::string::npos);
    CHECK(message.find("M0") != std::string::npos);
}
