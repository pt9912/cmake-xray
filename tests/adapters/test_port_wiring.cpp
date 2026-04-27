#include <doctest/doctest.h>

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include "adapters/cli/cli_adapter.h"
#include "adapters/cli/exit_codes.h"
#include "adapters/cli/output_verbosity.h"
#include "adapters/input/cmake_file_api_adapter.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "adapters/input/source_parsing_include_adapter.h"
#include "adapters/output/console_report_adapter.h"
#include "adapters/output/dot_report_adapter.h"
#include "adapters/output/html_report_adapter.h"
#include "adapters/output/json_report_adapter.h"
#include "adapters/output/markdown_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/ports/driving/generate_report_port.h"
#include "hexagon/services/impact_analyzer.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

TEST_CASE("driven input adapter satisfies build model port") {
    const xray::adapters::input::CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_build_model("tests/e2e/testdata/valid/compile_commands.json");

    CHECK(result.compile_database.is_success());
    CHECK(result.compile_database.entries().size() == 1);
    CHECK(result.source == xray::hexagon::model::ObservationSource::exact);
    CHECK(result.target_metadata == xray::hexagon::model::TargetMetadataStatus::not_loaded);
}

TEST_CASE("full M2 analyze pipeline wires from CLI through hexagon to adapters") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--top",
                          "2"};

    const int exit_code = cli.run(6, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(out.str().find("translation unit ranking") != std::string::npos);
    CHECK(out.str().find("top 2 of 3 translation units") != std::string::npos);
    CHECK(out.str().find("include hotspots [heuristic]") != std::string::npos);
    CHECK(out.str().find("include/common/config.h") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE("full M2 analyze pipeline is reproducible for permuted compile commands") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream baseline_out;
    std::ostringstream baseline_err;
    const char* baseline_argv[] = {"cmake-xray",
                                   "analyze",
                                   "--compile-commands",
                                   "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                                   "--top",
                                   "3"};

    REQUIRE(cli.run(6, baseline_argv, baseline_out, baseline_err) ==
            xray::adapters::cli::ExitCode::success);
    REQUIRE(baseline_err.str().empty());

    std::ostringstream permuted_out;
    std::ostringstream permuted_err;
    const char* permuted_argv[] = {
        "cmake-xray",
        "analyze",
        "--compile-commands",
        "tests/e2e/testdata/m2/permuted_compile_commands/compile_commands.json",
        "--top",
        "3",
    };

    REQUIRE(cli.run(6, permuted_argv, permuted_out, permuted_err) ==
            xray::adapters::cli::ExitCode::success);
    CHECK(permuted_out.str() == baseline_out.str());
    CHECK(permuted_err.str().empty());
}

TEST_CASE("full M2 impact pipeline wires from CLI through hexagon to adapters") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "impact", "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--changed-file", "include/common/shared.h"};

    const int exit_code = cli.run(6, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(out.str().find("impact analysis for include/common/shared.h [heuristic]") !=
          std::string::npos);
    CHECK(out.str().find("affected translation units: 3") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE("full M2 impact pipeline keeps duplicate translation-unit observations visible") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {
        "cmake-xray",
        "impact",
        "--compile-commands",
        "tests/e2e/testdata/m2/duplicate_tu_entries/compile_commands.json",
        "--changed-file",
        "src/app/main.cpp",
    };

    const int exit_code = cli.run(6, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(out.str().find("affected translation units: 2") != std::string::npos);
    CHECK(out.str().find("src/app/main.cpp [directory: build/debug] [direct]") !=
          std::string::npos);
    CHECK(out.str().find("src/app/main.cpp [directory: build/release] [direct]") !=
          std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE("full M3 markdown analyze pipeline wires from CLI through hexagon to adapters") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                          "markdown", "--top", "2"};

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(out.str().find("# Project Analysis Report") != std::string::npos);
    CHECK(out.str().find("- Report type: analyze") != std::string::npos);
    CHECK(out.str().find("## Include Hotspots") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE("full M3 markdown impact pipeline wires from CLI through hexagon to adapters") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {
        "cmake-xray",
        "impact",
        "--compile-commands",
        "tests/e2e/testdata/m2/basic_project/compile_commands.json",
        "--changed-file",
        "include/common/shared.h",
        "--format",
        "markdown",
    };

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(out.str().find("# Impact Analysis Report") != std::string::npos);
    CHECK(out.str().find("- Impact classification: heuristic") != std::string::npos);
    CHECK(out.str().find("## Heuristically Affected Translation Units") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE("full M5 json analyze pipeline reaches the JSON adapter, not the console fallback") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                          "json", "--top", "2"};

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("\"format\": \"cmake-xray.analysis\"") != std::string::npos);
    CHECK(out.str().find("\"report_type\": \"analyze\"") != std::string::npos);
    CHECK(out.str().find("translation unit ranking") == std::string::npos);
    CHECK(out.str().find("# Project Analysis Report") == std::string::npos);
}

TEST_CASE("full M5 json impact pipeline reaches the JSON adapter, not the console fallback") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {
        "cmake-xray", "impact", "--compile-commands",
        "tests/e2e/testdata/m2/basic_project/compile_commands.json",
        "--changed-file", "include/common/shared.h",
        "--format", "json",
    };

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("\"format\": \"cmake-xray.impact\"") != std::string::npos);
    CHECK(out.str().find("\"report_type\": \"impact\"") != std::string::npos);
    CHECK(out.str().find("# Impact Analysis Report") == std::string::npos);
}

TEST_CASE("full M5 dot analyze pipeline reaches the DOT adapter, not the console fallback") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                          "dot", "--top", "2"};

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("digraph cmake_xray_analysis {") != std::string::npos);
    CHECK(out.str().find("xray_report_type=\"analyze\"") != std::string::npos);
    CHECK(out.str().find("\"format\": \"cmake-xray.analysis\"") == std::string::npos);
    CHECK(out.str().find("# Project Analysis Report") == std::string::npos);
    CHECK(out.str().find("translation unit ranking") == std::string::npos);
}

TEST_CASE("full M5 dot impact pipeline reaches the DOT adapter, not the console fallback") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {
        "cmake-xray", "impact", "--compile-commands",
        "tests/e2e/testdata/m2/basic_project/compile_commands.json",
        "--changed-file", "include/common/shared.h",
        "--format", "dot",
    };

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("digraph cmake_xray_impact {") != std::string::npos);
    CHECK(out.str().find("xray_report_type=\"impact\"") != std::string::npos);
    CHECK(out.str().find("# Impact Analysis Report") == std::string::npos);
}

TEST_CASE("full M5 html analyze pipeline reaches the HTML adapter, not the console fallback") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                          "html", "--top", "2"};

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("<!doctype html>") != std::string::npos);
    CHECK(out.str().find("data-report-type=\"analyze\"") != std::string::npos);
    CHECK(out.str().find("<h1>cmake-xray analyze report</h1>") != std::string::npos);
    // Other format markers must not leak into the HTML stream.
    CHECK(out.str().find("digraph cmake_xray_analysis") == std::string::npos);
    CHECK(out.str().find("\"format\": \"cmake-xray.analysis\"") == std::string::npos);
    CHECK(out.str().find("# Project Analysis Report") == std::string::npos);
    CHECK(out.str().find("translation unit ranking") == std::string::npos);
    CHECK(out.str().find("recognized but not implemented") == std::string::npos);
}

TEST_CASE("full M5 html impact pipeline reaches the HTML adapter, not the console fallback") {
    const xray::adapters::input::CompileCommandsJsonAdapter compile_database_adapter;
    const xray::adapters::input::SourceParsingIncludeAdapter include_resolver_adapter;
    const xray::adapters::output::ConsoleReportAdapter console_report_adapter;
    const xray::adapters::output::MarkdownReportAdapter markdown_report_adapter;
    const xray::adapters::output::JsonReportAdapter json_report_adapter;
    const xray::adapters::output::DotReportAdapter dot_report_adapter;
    const xray::adapters::output::HtmlReportAdapter html_report_adapter;
    const xray::adapters::input::CmakeFileApiAdapter file_api_adapter;
    const xray::hexagon::services::ProjectAnalyzer project_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ImpactAnalyzer impact_analyzer{
        compile_database_adapter, include_resolver_adapter, file_api_adapter};
    const xray::hexagon::services::ReportGenerator console_report_generator{
        console_report_adapter};
    const xray::hexagon::services::ReportGenerator markdown_report_generator{
        markdown_report_adapter};
    const xray::hexagon::services::ReportGenerator json_report_generator{
        json_report_adapter};
    const xray::hexagon::services::ReportGenerator dot_report_generator{
        dot_report_adapter};
    const xray::hexagon::services::ReportGenerator html_report_generator{
        html_report_adapter};
    const xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                        markdown_report_generator,
                                                        json_report_generator,
                                                        dot_report_generator,
                                                        html_report_generator};
    const xray::adapters::cli::CliAdapter cli{project_analyzer, impact_analyzer, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {
        "cmake-xray", "impact", "--compile-commands",
        "tests/e2e/testdata/m2/basic_project/compile_commands.json",
        "--changed-file", "include/common/shared.h",
        "--format", "html",
    };

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code == xray::adapters::cli::ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("<!doctype html>") != std::string::npos);
    CHECK(out.str().find("data-report-type=\"impact\"") != std::string::npos);
    CHECK(out.str().find("<h1>cmake-xray impact report</h1>") != std::string::npos);
    CHECK(out.str().find("recognized but not implemented") == std::string::npos);
}

// AP M5-1.5 Tranche A: OutputVerbosity stays a CLI emission policy and never
// crosses into report adapters or driving ports. The plan's architectural
// invariant (cli/output_verbosity.h, paragraph "Voraussichtlich neue Datei")
// is checked here at compile time: every output adapter must remain default-
// constructible, and the GenerateReportPort interface must keep the documented
// signature without any verbosity argument. A future change that adds a
// verbosity parameter to an adapter constructor or to the port methods will
// break compilation of these static_asserts.
namespace {

static_assert(std::is_default_constructible_v<xray::adapters::output::ConsoleReportAdapter>,
              "AP M5-1.5: ConsoleReportAdapter must remain default-constructible "
              "without an OutputVerbosity argument");
static_assert(std::is_default_constructible_v<xray::adapters::output::MarkdownReportAdapter>,
              "AP M5-1.5: MarkdownReportAdapter must remain default-constructible "
              "without an OutputVerbosity argument");
static_assert(std::is_default_constructible_v<xray::adapters::output::JsonReportAdapter>,
              "AP M5-1.5: JsonReportAdapter must remain default-constructible "
              "without an OutputVerbosity argument");
static_assert(std::is_default_constructible_v<xray::adapters::output::DotReportAdapter>,
              "AP M5-1.5: DotReportAdapter must remain default-constructible "
              "without an OutputVerbosity argument");
static_assert(std::is_default_constructible_v<xray::adapters::output::HtmlReportAdapter>,
              "AP M5-1.5: HtmlReportAdapter must remain default-constructible "
              "without an OutputVerbosity argument");

// generate_analysis_report and generate_impact_report must keep their
// existing signatures. A future GenerateReportPort that adds an
// OutputVerbosity parameter would no longer satisfy these member-function
// type checks.
using AnalysisSignature = std::string (xray::hexagon::ports::driving::GenerateReportPort::*)(
    const xray::hexagon::model::AnalysisResult&, std::size_t) const;
using ImpactSignature = std::string (xray::hexagon::ports::driving::GenerateReportPort::*)(
    const xray::hexagon::model::ImpactResult&) const;
static_assert(std::is_same_v<AnalysisSignature,
                              decltype(&xray::hexagon::ports::driving::GenerateReportPort::
                                              generate_analysis_report)>,
              "AP M5-1.5: GenerateReportPort::generate_analysis_report must keep "
              "its (AnalysisResult, top_limit) signature without OutputVerbosity");
static_assert(std::is_same_v<ImpactSignature,
                              decltype(&xray::hexagon::ports::driving::GenerateReportPort::
                                              generate_impact_report)>,
              "AP M5-1.5: GenerateReportPort::generate_impact_report must keep "
              "its (ImpactResult) signature without OutputVerbosity");

}  // namespace

TEST_CASE("OutputVerbosity enum carries the three documented values") {
    // The enum is exercised at compile time via the static_asserts above; the
    // doctest assertion documents the value contract for readers and confirms
    // the underlying integer ordering stays normal < quiet < verbose.
    using xray::adapters::cli::OutputVerbosity;
    constexpr auto normal = static_cast<int>(OutputVerbosity::normal);
    constexpr auto quiet = static_cast<int>(OutputVerbosity::quiet);
    constexpr auto verbose = static_cast<int>(OutputVerbosity::verbose);
    CHECK(normal == 0);
    CHECK(quiet == 1);
    CHECK(verbose == 2);
}
