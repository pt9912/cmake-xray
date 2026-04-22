#include "adapters/cli/cli_adapter.h"

#include <cstddef>
#include <string>

#include <CLI/CLI.hpp>

#include "adapters/cli/exit_codes.h"
#include "hexagon/model/compile_database_result.h"

namespace xray::adapters::cli {

namespace {

using xray::hexagon::model::CompileDatabaseError;

struct CliOptions {
    std::string compile_commands_path;
    std::string changed_file_path;
    std::size_t top_limit{10};
};

struct OutputStreams {
    std::ostream& out;
    std::ostream& err;
};

int map_error_to_exit_code(CompileDatabaseError error) {
    switch (error) {
    case CompileDatabaseError::none:
        return ExitCode::success;
    case CompileDatabaseError::file_not_accessible:
        return ExitCode::input_not_accessible;
    case CompileDatabaseError::invalid_json:
    case CompileDatabaseError::not_an_array:
    case CompileDatabaseError::empty_database:
    case CompileDatabaseError::invalid_entries:
        return ExitCode::input_invalid;
    default:
        return ExitCode::unexpected_error;
    }
}

constexpr std::size_t max_displayed_entry_errors = 20;

void format_error(std::ostream& err, const xray::hexagon::model::CompileDatabaseResult& result) {
    err << "error: " << result.error_description() << '\n';

    if (!result.entry_diagnostics().empty()) {
        std::size_t displayed = 0;
        for (const auto& diag : result.entry_diagnostics()) {
            if (displayed >= max_displayed_entry_errors) break;
            err << "  entry " << diag.index() << ": " << diag.message() << '\n';
            ++displayed;
        }

        const auto total = result.total_invalid_entries();
        if (total > max_displayed_entry_errors) {
            err << "  ... and " << (total - max_displayed_entry_errors)
                << " more invalid entries\n";
        }
    }

    switch (result.error()) {
    case CompileDatabaseError::file_not_accessible:
        err << "hint: check the path or generate the compilation database before running "
               "cmake-xray analyze or impact\n";
        break;
    case CompileDatabaseError::empty_database:
        err << "hint: generate the compilation database before running cmake-xray analyze or "
               "impact\n";
        break;
    case CompileDatabaseError::invalid_json:
    case CompileDatabaseError::not_an_array:
    case CompileDatabaseError::invalid_entries:
        err << "hint: fix or regenerate the compilation database before running "
               "cmake-xray analyze or impact\n";
        break;
    default:
        break;
    }
}

void configure_analyze_command(CLI::App& app, CliOptions& options, CLI::App*& analyze_cmd) {
    analyze_cmd = app.add_subcommand("analyze", "Analyze a CMake project");
    analyze_cmd->add_option("--compile-commands", options.compile_commands_path,
                            "Path to compile_commands.json")
        ->required();
    analyze_cmd
        ->add_option("--top", options.top_limit,
                     "Limit ranking and hotspot output to the top N")
        ->default_val(options.top_limit)
        ->check(CLI::PositiveNumber);
}

void configure_impact_command(CLI::App& app, CliOptions& options, CLI::App*& impact_cmd) {
    impact_cmd = app.add_subcommand("impact", "Analyze the translation-unit impact of a file");
    impact_cmd->add_option("--compile-commands", options.compile_commands_path,
                           "Path to compile_commands.json")
        ->required();
    impact_cmd
        ->add_option("--changed-file", options.changed_file_path,
                     "Changed file path; relative paths are interpreted relative to the "
                     "provided compile_commands.json")
        ->required();
}

int handle_analysis_result(const xray::hexagon::model::AnalysisResult& result,
                           const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                           std::size_t top_limit, OutputStreams streams) {
    if (!result.compile_database.is_success()) {
        format_error(streams.err, result.compile_database);
        return map_error_to_exit_code(result.compile_database.error());
    }

    streams.out << report_port.generate_analysis_report(result, top_limit);
    return map_error_to_exit_code(result.compile_database.error());
}

int handle_impact_result(const xray::hexagon::model::ImpactResult& result,
                         const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                         OutputStreams streams) {
    if (!result.compile_database.is_success()) {
        format_error(streams.err, result.compile_database);
        return map_error_to_exit_code(result.compile_database.error());
    }

    streams.out << report_port.generate_impact_report(result);
    return map_error_to_exit_code(result.compile_database.error());
}

}  // namespace

CliAdapter::CliAdapter(
    const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port,
    const xray::hexagon::ports::driving::AnalyzeImpactPort& analyze_impact_port,
    const xray::hexagon::ports::driving::GenerateReportPort& generate_report_port)
    : analyze_project_port_(analyze_project_port),
      analyze_impact_port_(analyze_impact_port),
      generate_report_port_(generate_report_port) {}

int CliAdapter::run(int argc, const char* const* argv, std::ostream& out,
                    std::ostream& err) const {
    CLI::App app{"cmake-xray - build dependency analysis for CMake projects"};
    app.require_subcommand(0, 1);

    CliOptions options;
    CLI::App* analyze_cmd = nullptr;
    CLI::App* impact_cmd = nullptr;
    configure_analyze_command(app, options, analyze_cmd);
    configure_impact_command(app, options, impact_cmd);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        const int cli_exit = app.exit(e, out, err);
        return cli_exit == 0 ? ExitCode::success : ExitCode::cli_usage_error;
    }

    OutputStreams streams{out, err};
    if (impact_cmd->parsed()) {
        const auto result = analyze_impact_port_.analyze_impact(options.compile_commands_path,
                                                                options.changed_file_path);
        return handle_impact_result(result, generate_report_port_, streams);
    }

    if (!analyze_cmd->parsed()) {
        out << app.help();
        return ExitCode::success;
    }

    const auto result = analyze_project_port_.analyze_project(options.compile_commands_path);
    return handle_analysis_result(result, generate_report_port_, options.top_limit, streams);
}

}  // namespace xray::adapters::cli
