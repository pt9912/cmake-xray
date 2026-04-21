#include <doctest/doctest.h>

#include "adapters/cli/placeholder_cli_adapter.h"
#include "adapters/input/json_dependency_probe.h"

TEST_CASE("adapter dependencies are wired for later milestones") {
    CHECK(xray::adapters::input::json_dependency_ready());
    CHECK_FALSE(xray::adapters::cli::cli_dependency_status().empty());
}
