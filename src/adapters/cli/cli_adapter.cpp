#include "adapters/cli/cli_adapter.h"

#include <cerrno>
#include <cstdio>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include <CLI/CLI.hpp>
#include <unistd.h>

#include "adapters/cli/exit_codes.h"
#include "hexagon/model/compile_database_result.h"

namespace xray::adapters::cli {

namespace {

using xray::hexagon::model::CompileDatabaseError;

enum class ReportFormat {
    console,
    markdown,
};

struct CliOptions {
    std::string compile_commands_path;
    std::string changed_file_path;
    std::string output_path;
    std::string report_format{"console"};
    std::size_t top_limit{10};
};

struct OutputStreams {
    std::ostream& out;
    std::ostream& err;
};

struct ReportWriteError {
    std::string path;
    std::string reason;
};

int map_error_to_exit_code(CompileDatabaseError error) {
    switch (error) {
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

ReportFormat parse_report_format(std::string_view value) {
    return value == "markdown" ? ReportFormat::markdown : ReportFormat::console;
}

constexpr std::size_t max_displayed_entry_errors = 20;

void configure_report_options(CLI::App& command, CliOptions& options) {
    command
        .add_option("--format", options.report_format,
                    "Output format: console or markdown. Markdown without --output is written to stdout")
        ->default_val(options.report_format)
        ->check(CLI::IsMember({"console", "markdown"}));
    command.add_option("--output", options.output_path,
                       "Write a markdown report atomically to this path; valid only with --format markdown");
}

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
    configure_report_options(*analyze_cmd, options);
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
    configure_report_options(*impact_cmd, options);
}

std::optional<int> validate_report_options(const CliOptions& options, std::ostream& err) {
    if (options.output_path.empty() || options.report_format == "markdown") {
        return std::nullopt;
    }

    err << "error: --output is only supported with --format markdown\n";
    err << "hint: use --format markdown when writing a report file\n";
    return ExitCode::cli_usage_error;
}

std::string system_error_message(int error_number) {
    return std::error_code(error_number == 0 ? EIO : error_number, std::generic_category())
        .message();
}

std::filesystem::path report_output_directory(const std::filesystem::path& output_path) {
    const auto parent = output_path.parent_path();
    return parent.empty() ? std::filesystem::path{"."} : parent;
}

std::filesystem::path temporary_output_path(const std::filesystem::path& output_path,
                                            std::size_t attempt) {
    const auto file_name = output_path.filename().generic_string();
    const auto temp_name = ".cmake-xray-" + (file_name.empty() ? std::string{"report"} : file_name) +
                           "." + std::to_string(::getpid()) + "." + std::to_string(attempt) +
                           ".tmp";
    return report_output_directory(output_path) / temp_name;
}

std::optional<std::filesystem::path> reserve_temporary_output_path(
    const std::filesystem::path& output_path) {
    for (std::size_t attempt = 0; attempt < 64; ++attempt) {
        const auto candidate = temporary_output_path(output_path, attempt);
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec) && !ec) return candidate;
    }

    return std::nullopt;
}

void remove_temporary_output(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

std::optional<std::string> write_report_bytes(const std::filesystem::path& path,
                                              std::string_view report) {
    std::FILE* file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr) return system_error_message(errno);

    if (!report.empty()) {
        const auto written = std::fwrite(report.data(), 1, report.size(), file);
        // GCOVR_EXCL_START: deterministic stdio write faults would require fault injection.
        if (written != report.size()) {
            const auto error = system_error_message(errno);
            std::fclose(file);
            return error;
        }
        // GCOVR_EXCL_STOP
    }

    // GCOVR_EXCL_START: deterministic stdio flush faults would require fault injection.
    if (std::fflush(file) != 0) {
        const auto error = system_error_message(errno);
        std::fclose(file);
        return error;
    }
    // GCOVR_EXCL_STOP

    if (std::fclose(file) != 0) return system_error_message(errno);
    return std::nullopt;
}

std::optional<ReportWriteError> write_report_file(const std::string& output_path,
                                                  std::string_view report) {
    const std::filesystem::path target_path{output_path};
    const auto temp_path = reserve_temporary_output_path(target_path);
    if (!temp_path.has_value()) {
        return ReportWriteError{output_path, "cannot reserve a temporary report path"};
    }

    if (const auto error = write_report_bytes(*temp_path, report); error.has_value()) {
        remove_temporary_output(*temp_path);
        return ReportWriteError{output_path, *error};
    }

    std::error_code ec;
    std::filesystem::rename(*temp_path, target_path, ec);
    if (!ec) return std::nullopt;

    remove_temporary_output(*temp_path);
    return ReportWriteError{output_path, ec.message()};
}

int emit_report(const std::string& report, const CliOptions& options, OutputStreams streams) {
    if (options.output_path.empty()) {
        streams.out << report;
        return ExitCode::success;
    }

    const auto write_error = write_report_file(options.output_path, report);
    if (!write_error.has_value()) return ExitCode::success;

    streams.err << "error: cannot write report: " << write_error->path << ": "
                << write_error->reason << '\n';
    streams.err << "hint: check the output path and directory permissions\n";
    return ExitCode::unexpected_error;
}

const xray::hexagon::ports::driving::GenerateReportPort& select_report_port(
    ReportFormat format, const ReportPorts& report_ports) {
    return format == ReportFormat::markdown ? report_ports.markdown : report_ports.console;
}

int handle_analysis_result(const xray::hexagon::model::AnalysisResult& result,
                           const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                           const CliOptions& options, OutputStreams streams) {
    if (!result.compile_database.is_success()) {
        format_error(streams.err, result.compile_database);
        return map_error_to_exit_code(result.compile_database.error());
    }

    return emit_report(report_port.generate_analysis_report(result, options.top_limit), options,
                       streams);
}

int handle_impact_result(const xray::hexagon::model::ImpactResult& result,
                         const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                         const CliOptions& options, OutputStreams streams) {
    if (!result.compile_database.is_success()) {
        format_error(streams.err, result.compile_database);
        return map_error_to_exit_code(result.compile_database.error());
    }

    return emit_report(report_port.generate_impact_report(result), options, streams);
}

}  // namespace

CliAdapter::CliAdapter(
    const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port,
    const xray::hexagon::ports::driving::AnalyzeImpactPort& analyze_impact_port,
    ReportPorts report_ports)
    : analyze_project_port_(analyze_project_port),
      analyze_impact_port_(analyze_impact_port),
      report_ports_(report_ports) {}

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

    if (const auto validation_error = validate_report_options(options, err);
        validation_error.has_value()) {
        return *validation_error;
    }

    OutputStreams streams{out, err};
    const auto report_format = parse_report_format(options.report_format);
    const auto& report_port = select_report_port(report_format, report_ports_);

    if (impact_cmd->parsed()) {
        const auto result = analyze_impact_port_.analyze_impact(options.compile_commands_path,
                                                                options.changed_file_path);
        return handle_impact_result(result, report_port, options, streams);
    }

    if (!analyze_cmd->parsed()) {
        out << app.help();
        return ExitCode::success;
    }

    const auto result = analyze_project_port_.analyze_project(options.compile_commands_path);
    return handle_analysis_result(result, report_port, options, streams);
}

}  // namespace xray::adapters::cli
