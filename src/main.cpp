#include <iostream>

#include "adapters/cli/cli_adapter.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "adapters/input/source_parsing_include_adapter.h"
#include "adapters/output/console_report_adapter.h"
#include "hexagon/services/impact_analyzer.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

int main(int argc, char* argv[]) {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter report_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{compile_database_adapter,
                                                                    include_resolver_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{compile_database_adapter,
                                                                  include_resolver_adapter};
    const xray::hexagon::services::ReportGenerator report_generator{report_adapter};
    const xray::adapters::cli::CliAdapter cli_adapter{project_analyzer, impact_analyzer,
                                                      report_generator};

    return cli_adapter.run(argc, argv, std::cout, std::cerr);
}
