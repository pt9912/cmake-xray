#include "adapters/cli/cli_adapter.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include <CLI/CLI.hpp>

#include "adapters/cli/exit_codes.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/report_inputs.h"

namespace xray::adapters::cli {

namespace {

using xray::hexagon::model::CompileDatabaseError;

enum class ReportFormat {
    console,
    markdown,
    html,
    json,
    dot,
};

struct CliOptions {
    std::string compile_commands_path;
    std::string cmake_file_api_path;
    std::string changed_file_path;
    std::string output_path;
    std::string report_format{"console"};
    std::size_t top_limit{10};
};

int map_error_to_exit_code(CompileDatabaseError error) {
    switch (error) {
    case CompileDatabaseError::file_not_accessible:
    case CompileDatabaseError::file_api_not_accessible:
        return ExitCode::input_not_accessible;
    case CompileDatabaseError::invalid_json:
    case CompileDatabaseError::not_an_array:
    case CompileDatabaseError::empty_database:
    case CompileDatabaseError::invalid_entries:
    case CompileDatabaseError::file_api_invalid:
        return ExitCode::input_invalid;
    default:
        return ExitCode::unexpected_error;
    }
}

ReportFormat parse_report_format(std::string_view value) {
    if (value == "markdown") return ReportFormat::markdown;
    if (value == "html") return ReportFormat::html;
    if (value == "json") return ReportFormat::json;
    if (value == "dot") return ReportFormat::dot;
    return ReportFormat::console;
}

constexpr std::size_t max_displayed_entry_errors = 20;

void configure_report_options(CLI::App& command, CliOptions& options) {
    command
        .add_option("--format", options.report_format,
                    "Output format: console, markdown, html, json, or dot. "
                    "markdown without --output is written to stdout")
        ->default_val(options.report_format)
        ->check(CLI::IsMember({"console", "markdown", "html", "json", "dot"}));
    command.add_option(
        "--output", options.output_path,
        "Write a report atomically to this path; valid only with artifact-oriented formats");
}

void append_entry_diagnostics(std::ostream& err,
                              const xray::hexagon::model::CompileDatabaseResult& result) {
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
}

std::string_view error_hint(CompileDatabaseError error) {
    switch (error) {
    case CompileDatabaseError::file_not_accessible:
        return "hint: check the path or generate the compilation database before running "
               "cmake-xray analyze or impact";
    case CompileDatabaseError::empty_database:
        return "hint: generate the compilation database before running cmake-xray analyze or "
               "impact";
    case CompileDatabaseError::invalid_json:
    case CompileDatabaseError::not_an_array:
    case CompileDatabaseError::invalid_entries:
        return "hint: fix or regenerate the compilation database before running "
               "cmake-xray analyze or impact";
    case CompileDatabaseError::file_api_not_accessible:
        return "hint: check the --cmake-file-api path; provide either the build directory "
               "or the .cmake/api/v1/reply directory";
    case CompileDatabaseError::file_api_invalid:
        return "hint: ensure cmake has been configured with file api query files "
               "and that reply data is complete and up to date";
    default:
        return {};
    }
}

void format_error(std::ostream& err, const xray::hexagon::model::CompileDatabaseResult& result) {
    err << "error: " << result.error_description() << '\n';
    append_entry_diagnostics(err, result);

    const auto hint = error_hint(result.error());
    if (!hint.empty()) err << hint << '\n';
}

void configure_input_options(CLI::App& command, CliOptions& options) {
    command.add_option("--compile-commands", options.compile_commands_path,
                       "Path to compile_commands.json");
    command.add_option(
        "--cmake-file-api", options.cmake_file_api_path,
        "Path to CMake File API reply data: either a build directory containing "
        ".cmake/api/v1/reply or the reply directory itself. "
        "Provides target metadata; without --compile-commands, also serves as primary input source");
}

void configure_analyze_command(CLI::App& app, CliOptions& options, CLI::App*& analyze_cmd) {
    analyze_cmd = app.add_subcommand("analyze", "Analyze a CMake project");
    configure_input_options(*analyze_cmd, options);
    analyze_cmd
        ->add_option("--top", options.top_limit,
                     "Limit ranking and hotspot output to the top N")
        ->default_val(options.top_limit)
        ->check(CLI::PositiveNumber);
    configure_report_options(*analyze_cmd, options);
}

void configure_impact_command(CLI::App& app, CliOptions& options, CLI::App*& impact_cmd) {
    impact_cmd = app.add_subcommand("impact", "Analyze the translation-unit impact of a file");
    configure_input_options(*impact_cmd, options);
    // --changed-file is intentionally not parser-level required; the
    // changed-file-required check fires inside validate_subcommand_options
    // after format parsing so factual input validation runs in the
    // documented exit-code precedence (cli usage > input load > render).
    impact_cmd->add_option(
        "--changed-file", options.changed_file_path,
        "Changed file path; relative paths are interpreted relative to the "
        "compile_commands.json directory (with --compile-commands) or the "
        "top-level source directory from the codemodel (with --cmake-file-api only)");
    configure_report_options(*impact_cmd, options);
}

std::optional<int> validate_input_options(const CliOptions& options, std::ostream& err) {
    if (!options.compile_commands_path.empty() || !options.cmake_file_api_path.empty()) {
        return std::nullopt;
    }

    err << "error: at least one input source is required: "
           "--compile-commands or --cmake-file-api\n";
    err << "hint: provide --compile-commands for compile database analysis, "
           "--cmake-file-api for target metadata, or both\n";
    return ExitCode::cli_usage_error;
}

std::optional<int> validate_report_options(const CliOptions& options, std::ostream& err) {
    if (options.output_path.empty() || options.report_format != "console") {
        return std::nullopt;
    }
    err << "error: --output is not supported with --format console\n";
    err << "hint: use an artifact-oriented format such as --format markdown, "
           "--format json or --format dot when writing a report file\n";
    return ExitCode::cli_usage_error;
}

std::optional<int> validate_changed_file_required(const CliOptions& options, std::ostream& err) {
    if (!options.changed_file_path.empty()) return std::nullopt;
    err << "error: impact requires --changed-file\n";
    err << "hint: provide --changed-file <path>\n";
    return ExitCode::cli_usage_error;
}

const xray::hexagon::ports::driving::GenerateReportPort& select_report_port(
    ReportFormat format, const ReportPorts& report_ports) {
    // Explicit if-chain over every enumerator with a final console fallback.
    // CLI11's IsMember restricts --format to the documented set so this
    // function never sees an unmapped value at runtime; the chain is
    // arranged so a future enumerator forces a code edit here rather than
    // landing silently in the console branch.
    if (format == ReportFormat::markdown) return report_ports.markdown;
    if (format == ReportFormat::json) return report_ports.json;
    if (format == ReportFormat::dot) return report_ports.dot;
    if (format == ReportFormat::html) return report_ports.html;
    return report_ports.console;
}

int run_emit_for_renderer(const CliReportRenderer& renderer, const CliOptions& options,
                          CliOutputStreams streams) {
    DefaultAtomicFilePlatformOps ops;
    AtomicReportWriter writer{ops};
    return emit_rendered_report(renderer, options.output_path, writer, streams);
}

int handle_analysis_result(const xray::hexagon::model::AnalysisResult& result,
                           const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                           const CliOptions& options, CliOutputStreams streams) {
    if (!result.compile_database.is_success()) {
        format_error(streams.err, result.compile_database);
        return map_error_to_exit_code(result.compile_database.error());
    }
    const AnalysisCliReportRenderer renderer{report_port, result, options.top_limit};
    return run_emit_for_renderer(renderer, options, streams);
}

int handle_impact_result(const xray::hexagon::model::ImpactResult& result,
                         const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                         ReportFormat format, const CliOptions& options,
                         CliOutputStreams streams) {
    if (!result.compile_database.is_success()) {
        format_error(streams.err, result.compile_database);
        return map_error_to_exit_code(result.compile_database.error());
    }
    // docs/report-dot.md / docs/plan-M5-1-3.md: an ImpactResult whose
    // changed_file_source is unresolved_file_api_source_root is a file-api
    // error path. DOT must not render a graph for this case; the CLI emits
    // a text error and returns non-zero. Other formats (JSON / Markdown /
    // Console) continue to render so the caller can see the diagnostics.
    if (format == ReportFormat::dot && result.inputs.changed_file_source.has_value() &&
        *result.inputs.changed_file_source ==
            xray::hexagon::model::ChangedFileSource::unresolved_file_api_source_root) {
        streams.err << "error: cannot render --format dot when the file-api source "
                       "root is unresolved\n";
        streams.err << "hint: provide --compile-commands or a fully resolvable "
                       "--cmake-file-api path\n";
        return ExitCode::input_invalid;
    }
    const ImpactCliReportRenderer renderer{report_port, result};
    return run_emit_for_renderer(renderer, options, streams);
}

xray::hexagon::ports::driving::InputPathArgument make_input_path_argument(
    const std::string& cli_value, const std::filesystem::path& report_display_base) {
    const std::filesystem::path raw{cli_value};
    const bool was_relative = !raw.is_absolute();
    const auto path_for_io = was_relative
                                 ? (report_display_base / raw).lexically_normal()
                                 : raw.lexically_normal();
    return {raw, path_for_io, was_relative};
}

xray::hexagon::ports::driving::InputPathArgument make_changed_file_argument(
    const std::string& cli_value) {
    const std::filesystem::path raw{cli_value};
    const bool was_relative = !raw.is_absolute();
    const auto path_for_io = raw.lexically_normal();
    return {raw, path_for_io, was_relative};
}

std::optional<xray::hexagon::ports::driving::InputPathArgument> optional_input_path(
    const std::string& cli_value, const std::filesystem::path& report_display_base) {
    if (cli_value.empty()) return std::nullopt;
    return make_input_path_argument(cli_value, report_display_base);
}

xray::hexagon::ports::driving::AnalyzeProjectRequest build_project_request(
    const CliOptions& options, const std::filesystem::path& report_display_base) {
    // Aggregate return avoids a gcov NRVO artifact on the closing brace.
    return {optional_input_path(options.compile_commands_path, report_display_base),
            optional_input_path(options.cmake_file_api_path, report_display_base),
            report_display_base};
}

xray::hexagon::ports::driving::AnalyzeImpactRequest build_impact_request(
    const CliOptions& options, const std::filesystem::path& report_display_base) {
    // Aggregate return avoids a gcov NRVO artifact on the closing brace.
    return {optional_input_path(options.compile_commands_path, report_display_base),
            make_changed_file_argument(options.changed_file_path),
            optional_input_path(options.cmake_file_api_path, report_display_base),
            report_display_base};
}

std::optional<int> validate_subcommand_options(const CliOptions& options, bool is_impact,
                                                std::ostream& err) {
    if (const auto e = validate_report_options(options, err); e.has_value()) return e;
    if (is_impact) {
        if (const auto e = validate_changed_file_required(options, err); e.has_value()) return e;
    }
    return validate_input_options(options, err);
}

}  // namespace

int emit_rendered_report(const CliReportRenderer& renderer, std::string_view output_path,
                         AtomicReportWriter& writer, CliOutputStreams streams) {
    const auto result = renderer.render();
    if (result.error.has_value()) {
        streams.err << "error: cannot render report: " << result.error->message << '\n';
        return ExitCode::unexpected_error;
    }
    const auto& content = result.content.value_or(std::string{});
    if (output_path.empty()) {
        streams.out << content;
        return ExitCode::success;
    }
    const auto write_failure =
        writer.write_atomic(std::filesystem::path{output_path}, content);
    if (!write_failure.has_value()) return ExitCode::success;
    streams.err << "error: cannot write report: " << write_failure->path << ": "
                << write_failure->reason << '\n';
    streams.err << "hint: check the output path and directory permissions\n";
    return ExitCode::unexpected_error;
}

AnalysisCliReportRenderer::AnalysisCliReportRenderer(
    const xray::hexagon::ports::driving::GenerateReportPort& port,
    const xray::hexagon::model::AnalysisResult& result, std::size_t top_limit)
    : port_(&port), result_(&result), top_limit_(top_limit) {}

RenderResult AnalysisCliReportRenderer::render() const {
    try {
        return {port_->generate_analysis_report(*result_, top_limit_), std::nullopt};
    } catch (const std::exception& e) {
        return {std::nullopt, RenderError{e.what()}};
    }
}

ImpactCliReportRenderer::ImpactCliReportRenderer(
    const xray::hexagon::ports::driving::GenerateReportPort& port,
    const xray::hexagon::model::ImpactResult& result)
    : port_(&port), result_(&result) {}

RenderResult ImpactCliReportRenderer::render() const {
    try {
        return {port_->generate_impact_report(*result_), std::nullopt};
    } catch (const std::exception& e) {
        return {std::nullopt, RenderError{e.what()}};
    }
}

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

    if (!analyze_cmd->parsed() && !impact_cmd->parsed()) {
        out << app.help();
        return ExitCode::success;
    }

    if (const auto validation_error =
            validate_subcommand_options(options, impact_cmd->parsed(), err);
        validation_error.has_value()) {
        return *validation_error;
    }

    const CliOutputStreams streams{out, err};
    const auto report_format = parse_report_format(options.report_format);
    const auto& report_port = select_report_port(report_format, report_ports_);

    const auto report_display_base = std::filesystem::current_path();
    if (impact_cmd->parsed()) {
        const auto result = analyze_impact_port_.analyze_impact(
            build_impact_request(options, report_display_base));
        return handle_impact_result(result, report_port, report_format, options, streams);
    }

    const auto result = analyze_project_port_.analyze_project(
        build_project_request(options, report_display_base));
    return handle_analysis_result(result, report_port, options, streams);
}

}  // namespace xray::adapters::cli
