#include <iostream>

#include "adapters/cli/cli_adapter.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "hexagon/services/project_analyzer.h"

int main(int argc, char* argv[]) {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{compile_database_adapter};
    const xray::adapters::cli::CliAdapter cli_adapter{project_analyzer};

    return cli_adapter.run(argc, argv, std::cout, std::cerr);
}
