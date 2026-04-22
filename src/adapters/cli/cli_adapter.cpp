#include "adapters/cli/cli_adapter.h"

#include <string>

#include <CLI/CLI.hpp>

#include "adapters/cli/exit_codes.h"
#include "hexagon/model/compile_database_result.h"

namespace xray::adapters::cli {

namespace {

using xray::hexagon::model::CompileDatabaseError;

int map_error_to_exit_code(CompileDatabaseError error) {
    switch (error) {
    case CompileDatabaseError::file_not_accessible:
        return ExitCode::input_not_accessible;
    case CompileDatabaseError::invalid_json:
    case CompileDatabaseError::not_an_array:
    case CompileDatabaseError::empty_database:
    case CompileDatabaseError::invalid_entries:
        return ExitCode::input_invalid;
    case CompileDatabaseError::none:
        return ExitCode::success;
    }
    return ExitCode::unexpected_error;
}

constexpr std::size_t max_displayed_entry_errors = 20;

void format_error(std::ostream& err, const xray::hexagon::model::CompileDatabaseResult& result) {
    err << "error: " << result.error_description << '\n';

    if (!result.entry_diagnostics.empty()) {
        std::size_t displayed = 0;
        for (const auto& diag : result.entry_diagnostics) {
            if (displayed >= max_displayed_entry_errors) break;
            err << "  entry " << diag.index() << ": " << diag.message() << '\n';
            ++displayed;
        }

        const auto total = result.entry_diagnostics.size();
        if (total > max_displayed_entry_errors) {
            err << "  ... and " << (total - max_displayed_entry_errors)
                << " more invalid entries\n";
        }
    }

    switch (result.error) {
    case CompileDatabaseError::file_not_accessible:
        err << "hint: check the path or generate the compilation database before running "
               "cmake-xray analyze\n";
        break;
    case CompileDatabaseError::empty_database:
        err << "hint: generate the compilation database before running cmake-xray analyze\n";
        break;
    case CompileDatabaseError::invalid_json:
    case CompileDatabaseError::not_an_array:
    case CompileDatabaseError::invalid_entries:
        err << "hint: fix or regenerate the compilation database before running "
               "cmake-xray analyze\n";
        break;
    case CompileDatabaseError::none:
        break;
    }
}

}  // namespace

CliAdapter::CliAdapter(
    const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port)
    : analyze_project_port_(analyze_project_port) {}

int CliAdapter::run(int argc, const char* const* argv, std::ostream& out,
                    std::ostream& err) const {
    CLI::App app{"cmake-xray - build dependency analysis for CMake projects"};
    app.require_subcommand(0, 1);

    std::string compile_commands_path;
    auto* analyze_cmd = app.add_subcommand("analyze", "Analyze a CMake project");
    analyze_cmd->add_option("--compile-commands", compile_commands_path,
                            "Path to compile_commands.json")
        ->required();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        const int cli_exit = app.exit(e, out, err);
        return cli_exit == 0 ? ExitCode::success : ExitCode::cli_usage_error;
    }

    if (!analyze_cmd->parsed()) {
        out << app.help();
        return ExitCode::success;
    }

    const auto result = analyze_project_port_.analyze_project(compile_commands_path);

    if (!result.compile_database.is_success()) {
        format_error(err, result.compile_database);
        return map_error_to_exit_code(result.compile_database.error);
    }

    out << "compile database loaded: " << result.compile_database.entries.size() << " entries\n";
    return ExitCode::success;
}

}  // namespace xray::adapters::cli
