#include <iostream>

#include "adapters/cli/cli_adapter.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "adapters/input/source_parsing_include_adapter.h"
#include "adapters/output/console_report_adapter.h"
#include "adapters/output/markdown_report_adapter.h"
#include "hexagon/services/impact_analyzer.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

int main(int argc, char* argv[]) {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{compile_database_adapter,
                                                                    include_resolver_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{compile_database_adapter,
                                                                  include_resolver_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator};
    const xray::adapters::cli::CliAdapter cli_adapter{
        project_analyzer, impact_analyzer, report_ports};

    return cli_adapter.run(argc, argv, std::cout, std::cerr);
}
