#include <iostream>

#include "adapters/cli/placeholder_cli_adapter.h"
#include "adapters/input/json_dependency_probe.h"
#include "adapters/output/placeholder_report_adapter.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

int main() {
    const xray::adapters::input::JsonDependencyProbe compile_database_adapter;
    const xray::adapters::output::PlaceholderReportAdapter report_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{compile_database_adapter};
    const xray::hexagon::services::ReportGenerator report_generator{report_adapter};
    const xray::adapters::cli::PlaceholderCliAdapter cli_adapter{project_analyzer, report_generator};

    std::cout << cli_adapter.run() << '\n';
    return 0;
}
