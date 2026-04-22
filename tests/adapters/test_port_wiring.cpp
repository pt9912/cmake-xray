#include <doctest/doctest.h>

#include <sstream>
#include <string_view>

#include "adapters/cli/cli_adapter.h"
#include "adapters/cli/exit_codes.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "adapters/input/source_parsing_include_adapter.h"
#include "adapters/output/console_report_adapter.h"
#include "hexagon/services/impact_analyzer.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

TEST_CASE("driven input adapter satisfies compile database port") {
    const xray::adapters::input::CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database("tests/e2e/testdata/valid/compile_commands.json");

    CHECK(result.is_success());
    CHECK(result.entries().size() == 1);
}

TEST_CASE("full M2 analyze pipeline wires from CLI through hexagon to adapters") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter report_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{compile_database_adapter,
                                                                    include_resolver_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{compile_database_adapter,
                                                                  include_resolver_adapter};
    const xray::hexagon::services::ReportGenerator report_generator{report_adapter};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_generator};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "tests/e2e/testdata/m2/analyze/compile_commands.json", "--top", "2"};

    const int exit_code = cli.run(6, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(out.str().find("translation unit ranking") != std::string::npos);
    CHECK(out.str().find("top 2 of 3 translation units") != std::string::npos);
    CHECK(out.str().find("include hotspots [heuristic]") != std::string::npos);
    CHECK(out.str().find("include/common/config.h") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE("full M2 impact pipeline wires from CLI through hexagon to adapters") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter report_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{compile_database_adapter,
                                                                    include_resolver_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{compile_database_adapter,
                                                                  include_resolver_adapter};
    const xray::hexagon::services::ReportGenerator report_generator{report_adapter};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_generator};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "impact", "--compile-commands",
                          "tests/e2e/testdata/m2/analyze/compile_commands.json", "--changed-file",
                          "include/common/config.h"};

    const int exit_code = cli.run(6, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(out.str().find("impact analysis for include/common/config.h [heuristic]") !=
          std::string::npos);
    CHECK(out.str().find("affected translation units: 3") != std::string::npos);
    CHECK(err.str().empty());
}
