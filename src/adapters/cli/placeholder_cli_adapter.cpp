#include "adapters/cli/placeholder_cli_adapter.h"

#include <CLI/CLI.hpp>

namespace xray::adapters::cli {

std::string cli_dependency_status() {
    CLI::App app{"cmake-xray placeholder CLI adapter"};
    return app.get_description();
}

}  // namespace xray::adapters::cli
