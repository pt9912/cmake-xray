#include <doctest/doctest.h>

#include <sstream>
#include <string_view>

#include "adapters/cli/cli_adapter.h"
#include "adapters/cli/exit_codes.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "adapters/output/placeholder_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

TEST_CASE("driven input adapter satisfies compile database port") {
    const xray::adapters::input::CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database("tests/e2e/testdata/valid/compile_commands.json");

    CHECK(result.is_success());
    CHECK(result.entries.size() == 1);
}

TEST_CASE("full M1 pipeline wires from CLI through hexagon to adapter") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{compile_database_adapter};
    const xray::adapters::cli::CliAdapter cli{project_analyzer};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "tests/e2e/testdata/valid/compile_commands.json"};

    const int exit_code = cli.run(4, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(out.str().find("1 entries") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE("report generator still wires through report writer port") {
    const xray::adapters::output::PlaceholderReportAdapter report_adapter;
    const xray::hexagon::services::ReportGenerator generator{report_adapter};
    const xray::hexagon::model::AnalysisResult result{
        .application = xray::hexagon::model::application_info(),
        .compile_database = {},
    };

    CHECK(generator.generate_report(result) == "cmake-xray v0.2.0");
}
