#include "adapters/cli/cli_adapter.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <CLI/CLI.hpp>

#include "adapters/cli/cli_console_renderers.h"
#include "adapters/cli/exit_codes.h"
#include "adapters/cli/output_verbosity.h"
#include "hexagon/model/analysis_configuration.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/include_filter_options.h"
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
    // AP M5-1.5 Tranche A: command-local --quiet/--verbose flags. The two
    // booleans are intermediate parser state; resolve_verbosity() collapses
    // them into the single OutputVerbosity field after CLI11 parsing. Default
    // OutputVerbosity::normal keeps existing call sites byte-stable.
    bool quiet_flag{false};
    bool verbose_flag{false};
    OutputVerbosity verbosity{OutputVerbosity::normal};
    // AP M6-1.3 A.2: raw CLI text for --impact-target-depth. CLI11 captures
    // the user-supplied string verbatim; manual parsing in
    // validate_impact_target_depth produces the four documented error
    // phrases (negative value / not an integer / value exceeds maximum 32 /
    // option specified more than once). Empty text means "user did not
    // supply --impact-target-depth"; the service applies its default of 2.
    // The Option* pointer is captured in configure_impact_command so the
    // duplicate-occurrence check can read CLI11's parse-time count;
    // CLI11's ->each() callback only fires on the final value, not per
    // instance.
    std::string impact_target_depth_text;
    CLI::Option* impact_target_depth_option{nullptr};
    std::optional<std::size_t> parsed_impact_target_depth;
    // AP M6-1.3 A.2: --require-target-graph flag. When set and no usable
    // target graph is loaded, impact ends with exit 1 + the documented
    // error message (CLI maps the service code target_graph_required).
    bool require_target_graph_flag{false};
    // AP M6-1.4 A.3: --include-scope and --include-depth analyze options.
    // Both are single-value enums with default `all`; manual parsing in
    // validate_include_scope/_depth produces the three documented error
    // phrases each (invalid value / option specified more than once /
    // missing value). Empty text means the user did not supply the option,
    // and the parsed enum stays at the default `all`.
    std::string include_scope_text;
    CLI::Option* include_scope_option{nullptr};
    xray::hexagon::model::IncludeScope parsed_include_scope{
        xray::hexagon::model::IncludeScope::all};
    std::string include_depth_text;
    CLI::Option* include_depth_option{nullptr};
    xray::hexagon::model::IncludeDepthFilter parsed_include_depth{
        xray::hexagon::model::IncludeDepthFilter::all};
    // AP M6-1.5 A.1: --analysis <list> selects which analyze sections are
    // emitted. Raw text is captured so validate_analysis can split, trim,
    // dedup and resolve `all`; the parsed canonical sorted enum list lands
    // in parsed_analysis_sections and is then forwarded into
    // AnalyzeProjectRequest.analysis_sections.
    std::string analysis_text;
    CLI::Option* analysis_option{nullptr};
    std::vector<xray::hexagon::model::AnalysisSection> parsed_analysis_sections{
        xray::hexagon::model::AnalysisSection::tu_ranking,
        xray::hexagon::model::AnalysisSection::include_hotspots,
        xray::hexagon::model::AnalysisSection::target_graph,
        xray::hexagon::model::AnalysisSection::target_hubs,
    };
    // AP M6-1.5 A.1: --tu-threshold <metric>=<n> is multi-instance; CLI11
    // accumulates each occurrence into tu_threshold_texts so the validator
    // can emit the five plan-pinned error phrases by walking the raw inputs
    // in source order. parsed_tu_thresholds holds the validated map (at most
    // one entry per metric).
    std::vector<std::string> tu_threshold_texts;
    std::map<xray::hexagon::model::TuRankingMetric, std::size_t> parsed_tu_thresholds;
};

OutputVerbosity resolve_verbosity(const CliOptions& options) {
    // Mutual exclusion is enforced separately in validate_verbosity_options
    // and runs before this function. resolve_verbosity therefore assumes at
    // most one of the two flags is set.
    if (options.quiet_flag) return OutputVerbosity::quiet;
    if (options.verbose_flag) return OutputVerbosity::verbose;
    return OutputVerbosity::normal;
}

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

void configure_verbosity_options(CLI::App& command, CliOptions& options) {
    // AP M5-1.5 Tranche A: command-local --quiet and --verbose flags. The
    // mutual-exclusion check runs in validate_verbosity_options so the error
    // message is part of the documented Fehlervertrag (text on stderr,
    // ExitCode::cli_usage_error). CLI11's ->excludes() would also work, but
    // the explicit validator keeps the error wording within our control and
    // matches the precedence rules documented in plan-M5-1-5.md.
    command.add_flag("--quiet", options.quiet_flag,
                     "Reduce console output to a brief summary; reportinhaltlich a "
                     "noop for --format markdown, json, dot and html. "
                     "Mutually exclusive with --verbose.");
    command.add_flag("--verbose", options.verbose_flag,
                     "Emit additional diagnostic context for console reports and on "
                     "stderr for artifact formats. Mutually exclusive with --quiet.");
}

void configure_analyze_command(CLI::App& app, CliOptions& options, CLI::App*& analyze_cmd) {
    analyze_cmd = app.add_subcommand("analyze", "Analyze a CMake project");
    configure_input_options(*analyze_cmd, options);
    analyze_cmd
        ->add_option("--top", options.top_limit,
                     "Limit ranking and hotspot output to the top N")
        ->default_val(options.top_limit)
        ->check(CLI::PositiveNumber);
    // AP M6-1.4 A.3: --include-scope and --include-depth analyze filters.
    // Captured as raw strings so validate_include_scope/_depth can emit the
    // three documented error phrases each. take_last() lets CLI11 accept
    // duplicates silently so the validator owns the
    // "option specified more than once" phrasing via Option::count().
    options.include_scope_option = analyze_cmd->add_option(
        "--include-scope", options.include_scope_text,
        "Include hotspot origin filter: all (default), project, external, or unknown");
    options.include_scope_option->take_last();
    options.include_depth_option = analyze_cmd->add_option(
        "--include-depth", options.include_depth_text,
        "Include hotspot depth filter: all (default), direct, or indirect");
    options.include_depth_option->take_last();
    // AP M6-1.5 A.1: --analysis <list> selects which analyze sections are
    // emitted. Raw text is captured here; validate_analysis owns the six
    // documented error phrases.
    options.analysis_option = analyze_cmd->add_option(
        "--analysis", options.analysis_text,
        "Comma-separated analysis sections to emit: all (default), tu-ranking, "
        "include-hotspots, target-graph, target-hubs. target-hubs requires "
        "target-graph in the same list");
    options.analysis_option->take_last();
    // AP M6-1.5 A.1: --tu-threshold <metric>=<n> is multi-instance. CLI11
    // accumulates each occurrence into the raw-text vector so the
    // validator can re-emit the source-order spelling in the five
    // documented error phrases.
    analyze_cmd->add_option(
        "--tu-threshold", options.tu_threshold_texts,
        "TU-ranking metric threshold as <metric>=<n>; metric is one of "
        "arg_count, include_path_count, define_count. May be set once per "
        "metric");
    configure_report_options(*analyze_cmd, options);
    configure_verbosity_options(*analyze_cmd, options);
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
    // AP M6-1.3 A.2: reverse-target-graph traversal depth. Captured as
    // raw text so manual parsing can emit the four documented error
    // phrases; ->take_last() lets CLI11 accept duplicate occurrences
    // silently so validate_impact_target_depth (not CLI11) owns the
    // "option specified more than once" error phrase via the option's
    // post-parse count().
    options.impact_target_depth_option = impact_cmd->add_option(
        "--impact-target-depth", options.impact_target_depth_text,
        "Reverse-target-graph traversal depth for impact prioritisation "
        "(default 2, range 0-32)");
    options.impact_target_depth_option->take_last();
    impact_cmd->add_flag(
        "--require-target-graph", options.require_target_graph_flag,
        "Reject the impact run when no target graph data is available");
    configure_report_options(*impact_cmd, options);
    configure_verbosity_options(*impact_cmd, options);
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
    // AP M5-1.5 Tranche A: the hint wording is part of the documented
    // Fehlervertrag (plan lines 374-377). The list keeps every artifact-
    // oriented format from AP 1.2 / 1.3 / 1.4; previous wording omitted
    // --format html because HTML was not yet implemented in AP 1.1.
    err << "error: --output is not supported with --format console\n";
    err << "hint: use an artifact-oriented format such as --format markdown, "
           "--format json, --format dot or --format html when writing a report file\n";
    return ExitCode::cli_usage_error;
}

// AP M6-1.3 A.2: classify a non-empty --impact-target-depth text. The
// result is one of: a valid depth value, or one of the three text-form
// failure phrases. Cycle protection note: any non-empty input is
// classified in O(text.size()) without exceptions.
struct DepthParseOutcome {
    enum class Status { ok, not_an_integer, negative, out_of_range };
    Status status;
    std::size_t value;
};

DepthParseOutcome classify_depth_text(std::string_view text) {
    constexpr std::size_t kMaxDepth = 32;
    constexpr std::size_t kMaxDigits = 5;  // any 6+ digit number > kMaxDepth
    if (text.front() == '-') {
        return {DepthParseOutcome::Status::negative, 0};
    }
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return {DepthParseOutcome::Status::not_an_integer, 0};
        }
    }
    if (text.size() > kMaxDigits) {
        return {DepthParseOutcome::Status::out_of_range, 0};
    }
    std::size_t value = 0;
    for (const char ch : text) {
        value = value * 10 + static_cast<std::size_t>(ch - '0');
    }
    if (value > kMaxDepth) {
        return {DepthParseOutcome::Status::out_of_range, 0};
    }
    return {DepthParseOutcome::Status::ok, value};
}

std::string_view depth_error_phrase(DepthParseOutcome::Status status) {
    if (status == DepthParseOutcome::Status::negative) return "negative value";
    if (status == DepthParseOutcome::Status::not_an_integer) return "not an integer";
    return "value exceeds maximum 32";
}

// AP M6-1.3 A.2: parse and validate --impact-target-depth per
// docs/planning/plan-M6-1-3.md "CLI-Vertrag". Captures the parsed depth in
// options.parsed_impact_target_depth on success; emits one of the four
// documented error phrases on failure.
std::optional<int> validate_impact_target_depth(CliOptions& options, std::ostream& err) {
    if (options.impact_target_depth_option != nullptr &&
        options.impact_target_depth_option->count() > 1) {
        err << "error: --impact-target-depth: option specified more than once\n";
        return ExitCode::cli_usage_error;
    }
    if (options.impact_target_depth_text.empty()) {
        return std::nullopt;  // service applies default 2
    }
    const auto outcome = classify_depth_text(options.impact_target_depth_text);
    if (outcome.status == DepthParseOutcome::Status::ok) {
        options.parsed_impact_target_depth = outcome.value;
        return std::nullopt;
    }
    err << "error: --impact-target-depth: " << depth_error_phrase(outcome.status)
        << "\n";
    return ExitCode::cli_usage_error;
}

// AP M6-1.4 A.3: --include-scope and --include-depth validators.
// Each option produces two documented error phrases at this layer:
//   --include-<X>: invalid value '<value>'; allowed: <list>
//   --include-<X>: option specified more than once
// CLI11 itself rejects the typed-but-empty value form
// (e.g. `--include-scope=`) at parse time before this validator runs;
// the user-facing exit code is the same (cli_usage_error) but the exact
// phrase is CLI11's own. An option not passed at all leaves the parsed
// enum at its default `all` -- silently accepted.
std::optional<int> validate_include_scope(CliOptions& options, std::ostream& err) {
    if (options.include_scope_option == nullptr) return std::nullopt;
    if (options.include_scope_option->count() > 1) {
        err << "error: --include-scope: option specified more than once\n";
        return ExitCode::cli_usage_error;
    }
    if (options.include_scope_option->count() == 0) return std::nullopt;
    if (options.include_scope_text == "all") {
        options.parsed_include_scope = xray::hexagon::model::IncludeScope::all;
        return std::nullopt;
    }
    if (options.include_scope_text == "project") {
        options.parsed_include_scope = xray::hexagon::model::IncludeScope::project;
        return std::nullopt;
    }
    if (options.include_scope_text == "external") {
        options.parsed_include_scope = xray::hexagon::model::IncludeScope::external;
        return std::nullopt;
    }
    if (options.include_scope_text == "unknown") {
        options.parsed_include_scope = xray::hexagon::model::IncludeScope::unknown;
        return std::nullopt;
    }
    err << "error: --include-scope: invalid value '" << options.include_scope_text
        << "'; allowed: all, project, external, unknown\n";
    return ExitCode::cli_usage_error;
}

// AP M6-1.5 A.1: --analysis token tokenisation and lookup helpers.
//
// trim_ascii_whitespace mirrors the wording "ASCII-Whitespace-Trim" from
// plan-M6-1-5.md §320: leading and trailing spaces / tabs / cr / lf are
// stripped before the token is validated. The plan treats the resulting
// empty string as the "empty analysis value in list" error case.
std::string_view trim_ascii_whitespace(std::string_view value) {
    const auto is_ws = [](char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };
    std::size_t begin = 0;
    while (begin < value.size() && is_ws(value[begin])) ++begin;
    std::size_t end = value.size();
    while (end > begin && is_ws(value[end - 1])) --end;
    return value.substr(begin, end - begin);
}

std::optional<xray::hexagon::model::AnalysisSection> analysis_token_to_section(
    std::string_view token) {
    using xray::hexagon::model::AnalysisSection;
    if (token == "tu-ranking") return AnalysisSection::tu_ranking;
    if (token == "include-hotspots") return AnalysisSection::include_hotspots;
    if (token == "target-graph") return AnalysisSection::target_graph;
    if (token == "target-hubs") return AnalysisSection::target_hubs;
    return std::nullopt;
}

std::optional<int> validate_analysis(CliOptions& options, std::ostream& err) {
    using xray::hexagon::model::AnalysisSection;
    if (options.analysis_option == nullptr) return std::nullopt;
    if (options.analysis_option->count() > 1) {
        err << "error: --analysis: option specified more than once\n";
        return ExitCode::cli_usage_error;
    }
    if (options.analysis_option->count() == 0) return std::nullopt;

    // Split on `,`. Each token is trimmed; a fully-empty token after trim is
    // the "empty analysis value in list" error case from plan §337-338.
    std::vector<std::string_view> tokens;
    std::string_view remaining{options.analysis_text};
    while (true) {
        const auto comma = remaining.find(',');
        const auto raw = comma == std::string_view::npos ? remaining
                                                          : remaining.substr(0, comma);
        const auto trimmed = trim_ascii_whitespace(raw);
        if (trimmed.empty()) {
            err << "error: --analysis: empty analysis value in list\n";
            return ExitCode::cli_usage_error;
        }
        tokens.push_back(trimmed);
        if (comma == std::string_view::npos) break;
        remaining = remaining.substr(comma + 1);
    }

    // Validate each token; collect both the canonical-string form (for
    // duplicate / dependency error phrases) and the resolved enum.
    std::vector<std::string_view> canonical;
    std::vector<AnalysisSection> sections;
    bool saw_all = false;
    canonical.reserve(tokens.size());
    sections.reserve(tokens.size());
    for (const auto& token : tokens) {
        if (token == "all") {
            saw_all = true;
            canonical.push_back(token);
            continue;
        }
        const auto resolved = analysis_token_to_section(token);
        if (!resolved.has_value()) {
            err << "error: --analysis: unknown analysis '" << token
                << "'; allowed: all, tu-ranking, include-hotspots, target-graph, target-hubs\n";
            return ExitCode::cli_usage_error;
        }
        canonical.push_back(token);
        sections.push_back(*resolved);
    }

    // Dedup detection runs over the visible token strings so the error
    // phrase can quote the original spelling.
    for (std::size_t i = 0; i < canonical.size(); ++i) {
        for (std::size_t j = i + 1; j < canonical.size(); ++j) {
            if (canonical[i] == canonical[j]) {
                err << "error: --analysis: duplicate analysis value '" << canonical[i]
                    << "'\n";
                return ExitCode::cli_usage_error;
            }
        }
    }

    if (saw_all && canonical.size() > 1) {
        err << "error: --analysis: 'all' must not be combined with other analysis values\n";
        return ExitCode::cli_usage_error;
    }

    if (saw_all) {
        options.parsed_analysis_sections = {
            AnalysisSection::tu_ranking,
            AnalysisSection::include_hotspots,
            AnalysisSection::target_graph,
            AnalysisSection::target_hubs,
        };
        return std::nullopt;
    }

    // target-hubs without target-graph is a configuration conflict per
    // plan §334-336 (errors before input is even read).
    const auto has_hubs = std::find(sections.begin(), sections.end(),
                                    AnalysisSection::target_hubs) != sections.end();
    const auto has_graph = std::find(sections.begin(), sections.end(),
                                     AnalysisSection::target_graph) != sections.end();
    if (has_hubs && !has_graph) {
        err << "error: --analysis: target-hubs requires target-graph\n";
        return ExitCode::cli_usage_error;
    }

    // Canonical sort per plan §213-216 rank table; enum declaration order in
    // analysis_configuration.h matches the rank table (tu_ranking=0,
    // include_hotspots=1, target_graph=2, target_hubs=3).
    std::sort(sections.begin(), sections.end());
    options.parsed_analysis_sections = std::move(sections);
    return std::nullopt;
}

std::optional<xray::hexagon::model::TuRankingMetric> tu_metric_token_to_enum(
    std::string_view token) {
    using xray::hexagon::model::TuRankingMetric;
    if (token == "arg_count") return TuRankingMetric::arg_count;
    if (token == "include_path_count") return TuRankingMetric::include_path_count;
    if (token == "define_count") return TuRankingMetric::define_count;
    return std::nullopt;
}

std::optional<int> validate_tu_thresholds(CliOptions& options, std::ostream& err) {
    using xray::hexagon::model::TuRankingMetric;
    if (options.tu_threshold_texts.empty()) return std::nullopt;

    std::map<TuRankingMetric, std::size_t> parsed;
    for (const auto& raw : options.tu_threshold_texts) {
        const auto eq = raw.find('=');
        if (eq == std::string::npos) {
            err << "error: --tu-threshold: invalid syntax '" << raw
                << "'; expected <metric>=<n>\n";
            return ExitCode::cli_usage_error;
        }
        const std::string_view metric_text{raw.data(), eq};
        const std::string_view value_text{raw.data() + eq + 1, raw.size() - eq - 1};

        const auto metric = tu_metric_token_to_enum(metric_text);
        if (!metric.has_value()) {
            err << "error: --tu-threshold: unknown metric '" << metric_text
                << "'; allowed: arg_count, include_path_count, define_count\n";
            return ExitCode::cli_usage_error;
        }

        // Reject leading sign and any non-digit character. A leading '-'
        // shortcut to the "negative value" branch keeps the error phrase
        // closer to user intent than the generic "not an integer" phrase.
        if (!value_text.empty() && value_text.front() == '-') {
            err << "error: --tu-threshold: negative value\n";
            return ExitCode::cli_usage_error;
        }
        if (value_text.empty() ||
            !std::all_of(value_text.begin(), value_text.end(),
                         [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
            err << "error: --tu-threshold: not an integer\n";
            return ExitCode::cli_usage_error;
        }

        if (parsed.find(*metric) != parsed.end()) {
            err << "error: --tu-threshold: duplicate metric '" << metric_text << "'\n";
            return ExitCode::cli_usage_error;
        }

        std::size_t numeric_value = 0;
        for (const char ch : value_text) {
            numeric_value = numeric_value * 10 +
                            static_cast<std::size_t>(static_cast<unsigned char>(ch) - '0');
        }
        parsed.emplace(*metric, numeric_value);
    }

    options.parsed_tu_thresholds = std::move(parsed);
    return std::nullopt;
}

std::optional<int> validate_include_depth(CliOptions& options, std::ostream& err) {
    if (options.include_depth_option == nullptr) return std::nullopt;
    if (options.include_depth_option->count() > 1) {
        err << "error: --include-depth: option specified more than once\n";
        return ExitCode::cli_usage_error;
    }
    if (options.include_depth_option->count() == 0) return std::nullopt;
    if (options.include_depth_text == "all") {
        options.parsed_include_depth = xray::hexagon::model::IncludeDepthFilter::all;
        return std::nullopt;
    }
    if (options.include_depth_text == "direct") {
        options.parsed_include_depth = xray::hexagon::model::IncludeDepthFilter::direct;
        return std::nullopt;
    }
    if (options.include_depth_text == "indirect") {
        options.parsed_include_depth = xray::hexagon::model::IncludeDepthFilter::indirect;
        return std::nullopt;
    }
    err << "error: --include-depth: invalid value '" << options.include_depth_text
        << "'; allowed: all, direct, indirect\n";
    return ExitCode::cli_usage_error;
}

std::optional<int> validate_changed_file_required(const CliOptions& options, std::ostream& err) {
    if (!options.changed_file_path.empty()) return std::nullopt;
    err << "error: impact requires --changed-file\n";
    err << "hint: provide --changed-file <path>\n";
    return ExitCode::cli_usage_error;
}

std::optional<int> validate_verbosity_options(const CliOptions& options, std::ostream& err) {
    // AP M5-1.5 Tranche A: --quiet and --verbose are mutually exclusive at
    // the same subcommand. This check runs first inside
    // validate_subcommand_options so a usage-violating combination wins
    // before format-availability, --output, --changed-file and input
    // validation per the Fehlervertrag precedence list.
    if (!options.quiet_flag || !options.verbose_flag) return std::nullopt;
    err << "error: --quiet and --verbose are mutually exclusive\n";
    err << "hint: pass either --quiet or --verbose, not both\n";
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

// AP M5-1.5 Tranche C.1: Verbose-Fehlerkontext aus der CLI-Schicht. Die
// vier verbose:-Zeilen folgen exakt dem Fehlervertrag (plan ~407-410). stage
// wird vom Aufrufer gesetzt, nicht aus Exception-Typen abgeleitet.
struct VerboseErrorContext {
    std::string_view command;
    std::string_view format;
    std::string_view output;
    std::string_view stage;
};

void format_verbose_error_context(std::ostream& err, const VerboseErrorContext& ctx) {
    err << "verbose: command=" << ctx.command << '\n'
        << "verbose: format=" << ctx.format << '\n'
        << "verbose: output=" << ctx.output << '\n'
        << "verbose: validation_stage=" << ctx.stage << '\n';
}

std::string_view verbose_format_label(ReportFormat format) {
    if (format == ReportFormat::markdown) return "markdown";
    if (format == ReportFormat::json) return "json";
    if (format == ReportFormat::dot) return "dot";
    if (format == ReportFormat::html) return "html";
    return "console";
}

std::string_view verbose_output_label(const CliOptions& options) {
    return options.output_path.empty() ? "stdout" : "file";
}

std::string_view verbose_input_or_analysis_stage(CompileDatabaseError error) {
    if (error == CompileDatabaseError::file_not_accessible ||
        error == CompileDatabaseError::file_api_not_accessible) {
        return "input";
    }
    return "analysis";
}

VerboseErrorContext make_verbose_error_context(std::string_view command, ReportFormat format,
                                                 const CliOptions& options,
                                                 std::string_view stage) {
    return {command, verbose_format_label(format), verbose_output_label(options), stage};
}

int emit_rendered_report_with_verbose(const CliReportRenderer& renderer,
                                        std::string_view output_path,
                                        AtomicReportWriter& writer, CliOutputStreams streams,
                                        const VerboseErrorContext* verbose_ctx) {
    const auto result = renderer.render();
    if (result.error.has_value()) {
        streams.err << "error: cannot render report: " << result.error->message << '\n';
        if (verbose_ctx != nullptr) {
            const VerboseErrorContext ctx{verbose_ctx->command, verbose_ctx->format,
                                           verbose_ctx->output, "render"};
            format_verbose_error_context(streams.err, ctx);
        }
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
    if (verbose_ctx != nullptr) {
        const VerboseErrorContext ctx{verbose_ctx->command, verbose_ctx->format,
                                       verbose_ctx->output, "write"};
        format_verbose_error_context(streams.err, ctx);
    }
    return ExitCode::unexpected_error;
}

int run_emit_for_renderer(const CliReportRenderer& renderer, const CliOptions& options,
                          CliOutputStreams streams,
                          const VerboseErrorContext* verbose_ctx = nullptr) {
    DefaultAtomicFilePlatformOps ops;
    AtomicReportWriter writer{ops};
    return emit_rendered_report_with_verbose(renderer, options.output_path, writer, streams,
                                               verbose_ctx);
}

int finalize_compile_database_error(const xray::hexagon::model::CompileDatabaseResult& cdb,
                                      std::string_view command, ReportFormat format,
                                      const CliOptions& options, CliOutputStreams streams) {
    format_error(streams.err, cdb);
    if (options.verbosity == OutputVerbosity::verbose) {
        format_verbose_error_context(
            streams.err,
            make_verbose_error_context(command, format, options,
                                          verbose_input_or_analysis_stage(cdb.error())));
    }
    return map_error_to_exit_code(cdb.error());
}

// AP M5-1.5 Tranche B.2: helpers for the verbose: stderr block emitted after
// a successful artifact render. Console keeps stderr silent; markdown, json,
// dot and html each emit the documented 7-line (analyze) / 8-line (impact)
// verbose: prefix sequence in fixed order. top_limit is intentionally
// excluded from the artifact verbose block per plan ~362.
std::string_view artifact_format_label(ReportFormat format) {
    if (format == ReportFormat::markdown) return "markdown";
    if (format == ReportFormat::json) return "json";
    if (format == ReportFormat::dot) return "dot";
    return "html";
}

std::string_view artifact_output_label(const CliOptions& options) {
    return options.output_path.empty() ? "stdout" : "file";
}

std::string_view artifact_input_source_label(
    xray::hexagon::model::ReportInputSource source) {
    return source == xray::hexagon::model::ReportInputSource::cli ? "cli" : "not_provided";
}

std::string_view artifact_observation_source_label(
    xray::hexagon::model::ObservationSource source) {
    return source == xray::hexagon::model::ObservationSource::derived ? "derived" : "exact";
}

std::string_view artifact_target_metadata_label(
    xray::hexagon::model::TargetMetadataStatus status) {
    if (status == xray::hexagon::model::TargetMetadataStatus::loaded) return "loaded";
    if (status == xray::hexagon::model::TargetMetadataStatus::partial) return "partial";
    return "not_loaded";
}

std::string_view artifact_changed_file_source_label(
    xray::hexagon::model::ChangedFileSource source) {
    using xray::hexagon::model::ChangedFileSource;
    if (source == ChangedFileSource::compile_database_directory) {
        return "compile_database_directory";
    }
    if (source == ChangedFileSource::file_api_source_root) return "file_api_source_root";
    if (source == ChangedFileSource::cli_absolute) return "cli_absolute";
    return "unresolved_file_api_source_root";
}

bool should_emit_artifact_verbose_stderr(ReportFormat format, OutputVerbosity verbosity) {
    return format != ReportFormat::console && verbosity == OutputVerbosity::verbose;
}

struct ArtifactVerboseContext {
    std::string_view report_type;
    ReportFormat format;
    const CliOptions& options;
    const xray::hexagon::model::ReportInputs& inputs;
    xray::hexagon::model::ObservationSource observation_source;
    xray::hexagon::model::TargetMetadataStatus target_metadata;
};

void emit_artifact_verbose_stderr_common(std::ostream& err,
                                          const ArtifactVerboseContext& ctx) {
    err << "verbose: report_type=" << ctx.report_type << '\n'
        << "verbose: format=" << artifact_format_label(ctx.format) << '\n'
        << "verbose: output=" << artifact_output_label(ctx.options) << '\n'
        << "verbose: compile_database_source="
        << artifact_input_source_label(ctx.inputs.compile_database_source) << '\n'
        << "verbose: cmake_file_api_source="
        << artifact_input_source_label(ctx.inputs.cmake_file_api_source) << '\n'
        << "verbose: observation_source="
        << artifact_observation_source_label(ctx.observation_source) << '\n'
        << "verbose: target_metadata=" << artifact_target_metadata_label(ctx.target_metadata)
        << '\n';
}

void emit_artifact_verbose_stderr_analyze(
    std::ostream& err, ReportFormat format, const CliOptions& options,
    const xray::hexagon::model::AnalysisResult& result) {
    emit_artifact_verbose_stderr_common(
        err, ArtifactVerboseContext{"analyze", format, options, result.inputs,
                                      result.observation_source, result.target_metadata});
}

void emit_artifact_verbose_stderr_impact(
    std::ostream& err, ReportFormat format, const CliOptions& options,
    const xray::hexagon::model::ImpactResult& result) {
    emit_artifact_verbose_stderr_common(
        err, ArtifactVerboseContext{"impact", format, options, result.inputs,
                                      result.observation_source, result.target_metadata});
    // changed_file_source can be std::nullopt on synthetic impact results
    // that bypass the analyzer (e.g. doppelgaenger tests for non-html
    // artifact formats where the missing-changed-file precondition does not
    // fire). The "not_provided" fallback keeps the verbose block valid.
    err << "verbose: changed_file_source=";
    if (result.inputs.changed_file_source.has_value()) {
        err << artifact_changed_file_source_label(*result.inputs.changed_file_source);
    } else {
        err << "not_provided";
    }
    err << '\n';
}

int handle_analysis_artifact(const xray::hexagon::model::AnalysisResult& result,
                               const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                               ReportFormat format, const CliOptions& options,
                               CliOutputStreams streams) {
    const AnalysisCliReportRenderer renderer{report_port, result, options.top_limit};
    std::optional<VerboseErrorContext> ctx_storage;
    if (options.verbosity == OutputVerbosity::verbose) {
        ctx_storage = make_verbose_error_context("analyze", format, options, {});
    }
    const auto* ctx_ptr = ctx_storage.has_value() ? &*ctx_storage : nullptr;
    const auto exit_code = run_emit_for_renderer(renderer, options, streams, ctx_ptr);
    if (exit_code == ExitCode::success &&
        should_emit_artifact_verbose_stderr(format, options.verbosity)) {
        emit_artifact_verbose_stderr_analyze(streams.err, format, options, result);
    }
    return exit_code;
}

int handle_analysis_result(const xray::hexagon::model::AnalysisResult& result,
                           const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                           ReportFormat format, const CliOptions& options,
                           CliOutputStreams streams) {
    if (!result.compile_database.is_success()) {
        return finalize_compile_database_error(result.compile_database, "analyze", format, options,
                                                 streams);
    }
    if (format == ReportFormat::console && options.verbosity == OutputVerbosity::quiet) {
        const auto top_limit = options.top_limit;
        const StringFunctionCliReportRenderer renderer{
            [&result, top_limit] { return render_console_quiet_analyze(result, top_limit); }};
        return run_emit_for_renderer(renderer, options, streams);
    }
    if (format == ReportFormat::console && options.verbosity == OutputVerbosity::verbose) {
        const auto top_limit = options.top_limit;
        const StringFunctionCliReportRenderer renderer{
            [&result, top_limit] { return render_console_verbose_analyze(result, top_limit); }};
        return run_emit_for_renderer(renderer, options, streams);
    }
    return handle_analysis_artifact(result, report_port, format, options, streams);
}

// AP M5-1.5 Tranche B: factored render-precondition helper. The HTML adapter
// from AP 1.4 already rejected nullopt and unresolved_file_api_source_root,
// the DOT adapter from AP 1.3 rejected unresolved-only. Quiet/Verbose console
// shares the same render contract, so the predicate set is centralised here.
// Markdown and JSON keep rendering through to surface diagnostics.
std::string format_label_for_render_error(ReportFormat format, OutputVerbosity verbosity) {
    // Only called from within validate_impact_render_preconditions, which
    // gates entry on format_rejects_*; the predicates restrict the format/
    // verbosity combinations to (html), (dot) and (console + non-normal).
    // Markdown, JSON and console + normal-verbosity never reach this helper.
    if (format == ReportFormat::html) return "html";
    if (format == ReportFormat::dot) return "dot";
    if (verbosity == OutputVerbosity::quiet) return "console --quiet";
    return "console --verbose";
}

bool format_rejects_unresolved_file_api(ReportFormat format, OutputVerbosity verbosity) {
    // AP M5-1.8 A.5 audit fixup: plan-M5-1-8.md "1.8 Scope" bullet 43
    // pins JSON v1 to "Textfehler ohne JSON-Report" for the
    // unresolved_file_api_source_root model state, and bullet 22 demands
    // a CLI-level negative test for impact --format json (with and
    // without --output). Adding JSON to the precondition list keeps
    // the JSON-stdout and JSON-file paths aligned with the existing
    // DOT and HTML behaviour: render aborts before stdout or the
    // target file is touched.
    if (format == ReportFormat::dot || format == ReportFormat::html ||
        format == ReportFormat::json) {
        return true;
    }
    return format == ReportFormat::console && verbosity != OutputVerbosity::normal;
}

bool format_rejects_missing_changed_file(ReportFormat format, OutputVerbosity verbosity) {
    if (format == ReportFormat::html) return true;
    return format == ReportFormat::console && verbosity != OutputVerbosity::normal;
}

std::optional<int> validate_impact_render_preconditions(
    const xray::hexagon::model::ImpactResult& result, ReportFormat format,
    OutputVerbosity verbosity, std::ostream& err) {
    const bool unresolved_source = result.inputs.changed_file_source.has_value() &&
                                   *result.inputs.changed_file_source ==
                                       xray::hexagon::model::ChangedFileSource::
                                           unresolved_file_api_source_root;
    if (format_rejects_unresolved_file_api(format, verbosity) && unresolved_source) {
        err << "error: cannot render --format "
            << format_label_for_render_error(format, verbosity)
            << " when the file-api source root is unresolved\n";
        err << "hint: provide --compile-commands or a fully resolvable "
               "--cmake-file-api path\n";
        return ExitCode::input_invalid;
    }
    if (format_rejects_missing_changed_file(format, verbosity) &&
        !result.inputs.changed_file.has_value()) {
        err << "error: cannot render --format "
            << format_label_for_render_error(format, verbosity)
            << " when changed_file is missing from the impact result\n";
        err << "hint: rerun impact with --changed-file pointing at a concrete file\n";
        return ExitCode::input_invalid;
    }
    return std::nullopt;
}

int handle_impact_artifact(const xray::hexagon::model::ImpactResult& result,
                             const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                             ReportFormat format, const CliOptions& options,
                             CliOutputStreams streams) {
    const ImpactCliReportRenderer renderer{report_port, result};
    std::optional<VerboseErrorContext> ctx_storage;
    if (options.verbosity == OutputVerbosity::verbose) {
        ctx_storage = make_verbose_error_context("impact", format, options, {});
    }
    const auto* ctx_ptr = ctx_storage.has_value() ? &*ctx_storage : nullptr;
    const auto exit_code = run_emit_for_renderer(renderer, options, streams, ctx_ptr);
    if (exit_code == ExitCode::success &&
        should_emit_artifact_verbose_stderr(format, options.verbosity)) {
        emit_artifact_verbose_stderr_impact(streams.err, format, options, result);
    }
    return exit_code;
}

int handle_impact_result(const xray::hexagon::model::ImpactResult& result,
                         const xray::hexagon::ports::driving::GenerateReportPort& report_port,
                         ReportFormat format, const CliOptions& options,
                         CliOutputStreams streams) {
    if (!result.compile_database.is_success()) {
        return finalize_compile_database_error(result.compile_database, "impact", format, options,
                                                 streams);
    }
    if (const auto e = validate_impact_render_preconditions(result, format, options.verbosity,
                                                             streams.err);
        e.has_value()) {
        return *e;
    }
    if (format == ReportFormat::console && options.verbosity == OutputVerbosity::quiet) {
        const StringFunctionCliReportRenderer renderer{
            [&result] { return render_console_quiet_impact(result); }};
        return run_emit_for_renderer(renderer, options, streams);
    }
    if (format == ReportFormat::console && options.verbosity == OutputVerbosity::verbose) {
        const StringFunctionCliReportRenderer renderer{
            [&result] { return render_console_verbose_impact(result); }};
        return run_emit_for_renderer(renderer, options, streams);
    }
    return handle_impact_artifact(result, report_port, format, options, streams);
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
            report_display_base,
            options.parsed_include_scope,
            options.parsed_include_depth,
            options.parsed_analysis_sections,
            options.parsed_tu_thresholds,
            /*min_hotspot_tus=*/2,
            /*target_hub_in_threshold=*/10,
            /*target_hub_out_threshold=*/10};
}

xray::hexagon::ports::driving::AnalyzeImpactRequest build_impact_request(
    const CliOptions& options, const std::filesystem::path& report_display_base) {
    // Aggregate return avoids a gcov NRVO artifact on the closing brace.
    return {optional_input_path(options.compile_commands_path, report_display_base),
            make_changed_file_argument(options.changed_file_path),
            optional_input_path(options.cmake_file_api_path, report_display_base),
            report_display_base,
            options.parsed_impact_target_depth,
            options.require_target_graph_flag};
}

std::optional<int> validate_subcommand_options(CliOptions& options, bool is_impact,
                                                std::ostream& err) {
    if (const auto e = validate_verbosity_options(options, err); e.has_value()) return e;
    if (const auto e = validate_report_options(options, err); e.has_value()) return e;
    if (is_impact) {
        if (const auto e = validate_changed_file_required(options, err); e.has_value()) return e;
        if (const auto e = validate_impact_target_depth(options, err); e.has_value()) return e;
    } else {
        if (const auto e = validate_include_scope(options, err); e.has_value()) return e;
        if (const auto e = validate_include_depth(options, err); e.has_value()) return e;
        if (const auto e = validate_analysis(options, err); e.has_value()) return e;
        if (const auto e = validate_tu_thresholds(options, err); e.has_value()) return e;
    }
    return validate_input_options(options, err);
}

struct CliDispatchContext {
    const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port;
    const xray::hexagon::ports::driving::AnalyzeImpactPort& analyze_impact_port;
    const ReportPorts& report_ports;
};

// AP M6-1.3 A.2: map a service-side AnalyzeImpactRequest rejection to
// the documented CLI exit code and stderr phrase. The CLI's own
// validate_impact_target_depth catches depth issues before the request
// reaches the service, so the only service-validation code that can
// surface here is target_graph_required (triggered by
// --require-target-graph + missing target graph).
//
// Plan-M6-1-3.md "Exit-Code-Begruendung": exit 1 marks the input
// failure mode -- the CLI parser was syntactically clean, the request
// became invalid only after the build model load produced
// target_graph_status=not_loaded. The numeric value is the
// plan-mandated 1 (mapped to ExitCode::unexpected_error in
// src/adapters/cli/exit_codes.h despite the name).
int handle_impact_service_validation_error(
    const xray::hexagon::model::ServiceValidationError& info,
    std::ostream& err) {
    err << "error: --require-target-graph: " << info.message << "\n";
    return ExitCode::unexpected_error;
}

int dispatch_subcommand(const CliOptions& options, bool is_impact,
                          const CliDispatchContext& ctx, CliOutputStreams streams) {
    const auto report_format = parse_report_format(options.report_format);
    const auto& report_port = select_report_port(report_format, ctx.report_ports);
    const auto report_display_base = std::filesystem::current_path();
    if (is_impact) {
        const auto result = ctx.analyze_impact_port.analyze_impact(
            build_impact_request(options, report_display_base));
        if (result.service_validation_error.has_value()) {
            return handle_impact_service_validation_error(
                *result.service_validation_error, streams.err);
        }
        return handle_impact_result(result, report_port, report_format, options, streams);
    }
    const auto result = ctx.analyze_project_port.analyze_project(
        build_project_request(options, report_display_base));
    return handle_analysis_result(result, report_port, report_format, options, streams);
}

}  // namespace

int emit_rendered_report(const CliReportRenderer& renderer, std::string_view output_path,
                         AtomicReportWriter& writer, CliOutputStreams streams) {
    return emit_rendered_report_with_verbose(renderer, output_path, writer, streams, nullptr);
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
    // AP M5-1.6 Tranche A: app description plus an explicit app name so the
    // Usage line in `--help` is always "cmake-xray [OPTIONS] [SUBCOMMAND]"
    // regardless of how argv[0] arrives. Without an explicit name CLI11 falls
    // back to argv[0] verbatim; on the GitHub-Actions Linux runner the
    // Native CI shell smoke observed an argv[0] that did not survive into the
    // help output, while macOS and Windows runners did. Setting the name
    // upfront makes the help-text contract independent of that detail.
    CLI::App app{"cmake-xray - build dependency analysis for CMake projects",
                  "cmake-xray"};
    app.require_subcommand(0, 1);
    // AP M5-1.6 Tranche A: global --version flag handled by CLI11 before
    // subcommand dispatch; CLI::CallForVersion is caught as success below.
    app.set_version_flag("--version",
                         std::string{xray::hexagon::model::application_info().version});

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
    options.verbosity = resolve_verbosity(options);
    const CliDispatchContext ctx{analyze_project_port_, analyze_impact_port_, report_ports_};
    return dispatch_subcommand(options, impact_cmd->parsed(), ctx, CliOutputStreams{out, err});
}

}  // namespace xray::adapters::cli
