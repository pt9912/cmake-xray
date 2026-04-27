#include <doctest/doctest.h>

#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <csignal>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "adapters/cli/atomic_report_writer.h"
#include "adapters/cli/cli_adapter.h"
#include "adapters/cli/cli_report_renderer.h"
#include "adapters/cli/exit_codes.h"
#include "adapters/input/cmake_file_api_adapter.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "adapters/input/source_parsing_include_adapter.h"
#include "adapters/output/console_report_adapter.h"
#include "adapters/output/dot_report_adapter.h"
#include "adapters/output/html_report_adapter.h"
#include "adapters/output/json_report_adapter.h"
#include "adapters/output/markdown_report_adapter.h"
#include "hexagon/model/application_info.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/ports/driving/analyze_impact_port.h"
#include "hexagon/ports/driving/analyze_project_port.h"
#include "hexagon/ports/driving/generate_report_port.h"
#include "hexagon/services/impact_analyzer.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

namespace {

using xray::adapters::cli::CliAdapter;
using xray::adapters::cli::ExitCode;
using xray::adapters::input::CmakeFileApiAdapter;
using xray::adapters::input::CompileCommandsJsonAdapter;
using xray::adapters::input::SourceParsingIncludeAdapter;
using xray::adapters::output::ConsoleReportAdapter;
using xray::adapters::output::DotReportAdapter;
using xray::adapters::output::HtmlReportAdapter;
using xray::adapters::output::JsonReportAdapter;
using xray::adapters::output::MarkdownReportAdapter;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::EntryDiagnostic;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::services::ImpactAnalyzer;
using xray::hexagon::services::ProjectAnalyzer;
using xray::hexagon::services::ReportGenerator;

struct CliFixture {
    CompileCommandsJsonAdapter compile_database_adapter;
    CmakeFileApiAdapter file_api_adapter;
    SourceParsingIncludeAdapter include_resolver_adapter;
    ConsoleReportAdapter console_report_adapter;
    MarkdownReportAdapter markdown_report_adapter;
    JsonReportAdapter json_report_adapter;
    DotReportAdapter dot_report_adapter;
    HtmlReportAdapter html_report_adapter;
    ProjectAnalyzer project_analyzer{compile_database_adapter, include_resolver_adapter,
                                     file_api_adapter};
    ImpactAnalyzer impact_analyzer{compile_database_adapter, include_resolver_adapter,
                                   file_api_adapter};
    ReportGenerator console_report_generator{console_report_adapter};
    ReportGenerator markdown_report_generator{markdown_report_adapter};
    ReportGenerator json_report_generator{json_report_adapter};
    ReportGenerator dot_report_generator{dot_report_adapter};
    ReportGenerator html_report_generator{html_report_adapter};
    xray::adapters::cli::ReportPorts report_ports{console_report_generator,
                                                  markdown_report_generator,
                                                  json_report_generator,
                                                  dot_report_generator,
                                                  html_report_generator};
    CliAdapter cli{project_analyzer, impact_analyzer, report_ports};
    std::ostringstream out;
    std::ostringstream err;

    int run(std::initializer_list<const char*> args) {
        out.str({});
        out.clear();
        err.str({});
        err.clear();

        std::vector<const char*> argv_vec = {"cmake-xray"};
        argv_vec.insert(argv_vec.end(), args);
        return cli.run(static_cast<int>(argv_vec.size()), argv_vec.data(), out, err);
    }
};

const std::string testdata = "tests/e2e/testdata/";

std::string fixture_path(std::string_view relative_path) {
    return testdata + std::string(relative_path);
}

class TemporaryDirectory {
public:
    TemporaryDirectory() : path_(make_unique_path()) { std::filesystem::create_directories(path_); }

    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    static std::filesystem::path make_unique_path() {
        static std::size_t counter = 0;
        return std::filesystem::temp_directory_path() /
               ("cmake-xray-cli-" + std::to_string(counter++));
    }

    std::filesystem::path path_;
};

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
}

bool contains_temporary_report_file(const std::filesystem::path& directory) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().filename().string().starts_with(".cmake-xray-")) return true;
    }

    return false;
}

class StubAnalyzeProjectPort final : public xray::hexagon::ports::driving::AnalyzeProjectPort {
public:
    explicit StubAnalyzeProjectPort(AnalysisResult result) : result_(std::move(result)) {}

    AnalysisResult analyze_project(
        xray::hexagon::ports::driving::AnalyzeProjectRequest /*request*/) const override {
        return result_;
    }

private:
    AnalysisResult result_;
};

class StubAnalyzeImpactPort final : public xray::hexagon::ports::driving::AnalyzeImpactPort {
public:
    ImpactResult analyze_impact(
        xray::hexagon::ports::driving::AnalyzeImpactRequest /*request*/) const override {
        return {};
    }
};

class StubGenerateReportPort final : public xray::hexagon::ports::driving::GenerateReportPort {
public:
    std::string generate_analysis_report(const AnalysisResult&, std::size_t) const override {
        return {};
    }

    std::string generate_impact_report(const ImpactResult&) const override { return {}; }
};

}  // namespace

TEST_CASE_FIXTURE(CliFixture, "no subcommand returns exit 0 with help on stdout") {
    CHECK(run({}) == ExitCode::success);
    CHECK(out.str().find("cmake-xray") != std::string::npos);
    CHECK(out.str().find("analyze") != std::string::npos);
    CHECK(out.str().find("impact") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze --help returns exit 0 with help on stdout") {
    CHECK(run({"analyze", "--help"}) == ExitCode::success);
    CHECK(out.str().find("--compile-commands") != std::string::npos);
    CHECK(out.str().find("--cmake-file-api") != std::string::npos);
    CHECK(out.str().find("--top") != std::string::npos);
    CHECK(out.str().find("--format") != std::string::npos);
    CHECK(out.str().find("--output") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "impact --help returns exit 0 with help on stdout") {
    CHECK(run({"impact", "--help"}) == ExitCode::success);
    CHECK(out.str().find("--compile-commands") != std::string::npos);
    CHECK(out.str().find("--cmake-file-api") != std::string::npos);
    CHECK(out.str().find("--changed-file") != std::string::npos);
    CHECK(out.str().find("--format") != std::string::npos);
    CHECK(out.str().find("--output") != std::string::npos);
    CHECK(out.str().find("relative paths are interpreted relative") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze success path renders ranking and hotspots") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");

    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--top", "2"}) ==
          ExitCode::success);
    CHECK(out.str().find("translation unit ranking") != std::string::npos);
    CHECK(out.str().find("top 2 of 3 translation units") != std::string::npos);
    CHECK(out.str().find("include hotspots [heuristic]") != std::string::npos);
    CHECK(out.str().find("warning: could not resolve include") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "impact success path for direct source match is not heuristic") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");

    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp"}) == ExitCode::success);
    CHECK(out.str().find("impact analysis for src/app/main.cpp") != std::string::npos);
    CHECK(out.str().find("[heuristic]") == std::string::npos);
    CHECK(out.str().find("affected translation units: 1") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact success path for transitive header match is heuristic") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");

    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "include/common/shared.h"}) == ExitCode::success);
    CHECK(out.str().find("impact analysis for include/common/shared.h [heuristic]") !=
          std::string::npos);
    CHECK(out.str().find("affected translation units: 3") != std::string::npos);
    CHECK(out.str().find("warning: could not resolve include") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact path semantics normalize lexical relative paths") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");

    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "./include/common/../common/config.h"}) == ExitCode::success);
    CHECK(out.str().find("impact analysis for include/common/config.h [heuristic]") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact missing file keeps exit 0 and explains missing data") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");

    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "include/generated/version.h"}) == ExitCode::success);
    CHECK(out.str().find("affected translation units: 0") != std::string::npos);
    CHECK(out.str().find("not present in the loaded compile database") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "analyze output stays identical for permuted compile commands") {
    const auto baseline_compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    const auto permuted_compile_commands =
        fixture_path("m2/permuted_compile_commands/compile_commands.json");

    REQUIRE(run({"analyze", "--compile-commands", baseline_compile_commands.c_str(), "--top",
                 "3"}) == ExitCode::success);
    const auto baseline_output = out.str();

    REQUIRE(run({"analyze", "--compile-commands", permuted_compile_commands.c_str(), "--top",
                 "3"}) == ExitCode::success);
    CHECK(out.str() == baseline_output);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "markdown analyze writes report to stdout by default") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");

    REQUIRE(run({"analyze", "--compile-commands", compile_commands.c_str(), "--format",
                 "markdown", "--top", "2"}) == ExitCode::success);
    CHECK(out.str().find("# Project Analysis Report") != std::string::npos);
    CHECK(out.str().find("- Report type: analyze") != std::string::npos);
    CHECK(out.str().find("## Translation Unit Ranking") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "markdown impact writes report to stdout by default") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");

    REQUIRE(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
                 "include/common/shared.h", "--format", "markdown"}) == ExitCode::success);
    CHECK(out.str().find("# Impact Analysis Report") != std::string::npos);
    CHECK(out.str().find("- Impact classification: heuristic") != std::string::npos);
    CHECK(out.str().find("## Heuristically Affected Translation Units") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "markdown analyze can write atomically to an output file") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    const TemporaryDirectory temp_dir;
    const auto report_path = temp_dir.path() / "analyze-report.md";
    const auto report_path_text = report_path.string();

    REQUIRE(run({"analyze", "--compile-commands", compile_commands.c_str(), "--format",
                 "markdown", "--output", report_path_text.c_str(), "--top", "2"}) ==
            ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    CHECK(std::filesystem::is_regular_file(report_path));
    CHECK(read_text_file(report_path).find("# Project Analysis Report") != std::string::npos);
    CHECK(read_text_file(report_path).find("- Top limit: 2") != std::string::npos);
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE_FIXTURE(CliFixture, "output is rejected with the default console format") {
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--output",
               "report.md"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--output is not supported with --format console") !=
          std::string::npos);
    CHECK(err.str().find("use an artifact-oriented format") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "console output rejection wins over a missing input source") {
    const TemporaryDirectory temp_dir;
    const auto target_path = (temp_dir.path() / "report.md").string();

    CHECK(run({"analyze", "--format", "console", "--output", target_path.c_str()}) ==
          ExitCode::cli_usage_error);
    CHECK(err.str().find("--output is not supported with --format console") !=
          std::string::npos);
    CHECK(err.str().find("at least one input source") == std::string::npos);
    CHECK_FALSE(std::filesystem::exists(target_path));
}

TEST_CASE_FIXTURE(CliFixture, "markdown output write failures return exit 1 without leaving temp files") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    const TemporaryDirectory temp_dir;
    const auto target_directory = temp_dir.path() / "report-target";
    std::filesystem::create_directories(target_directory);
    const auto target_path_text = target_directory.string();

    REQUIRE(run({"analyze", "--compile-commands", compile_commands.c_str(), "--format",
                 "markdown", "--output", target_path_text.c_str()}) ==
            ExitCode::unexpected_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("error: cannot write report:") != std::string::npos);
    CHECK(err.str().find("hint: check the output path and directory permissions") !=
          std::string::npos);
    CHECK(std::filesystem::is_directory(target_directory));
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE_FIXTURE(CliFixture, "markdown output reports missing directories as write failures") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    const TemporaryDirectory temp_dir;
    const auto target_path = temp_dir.path() / "missing" / "report.md";
    const auto target_path_text = target_path.string();

    REQUIRE(run({"analyze", "--compile-commands", compile_commands.c_str(), "--format",
                 "markdown", "--output", target_path_text.c_str()}) ==
            ExitCode::unexpected_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("error: cannot write report:") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(target_path));
}

TEST_CASE_FIXTURE(CliFixture, "markdown output reports exhausted temporary report slots") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    const TemporaryDirectory temp_dir;
    const auto target_path = temp_dir.path() / "report.md";
    const auto target_name = target_path.filename().string();
#ifdef _WIN32
    const auto process_id = std::to_string(::_getpid());
#else
    const auto process_id = std::to_string(::getpid());
#endif

    for (std::size_t attempt = 0; attempt < 64; ++attempt) {
        std::ofstream reserved(temp_dir.path() /
                               (".cmake-xray-" + target_name + "." + process_id + "." +
                                std::to_string(attempt) + ".tmp"));
        reserved << "occupied";
    }

    const auto target_path_text = target_path.string();
    REQUIRE(run({"analyze", "--compile-commands", compile_commands.c_str(), "--format",
                 "markdown", "--output", target_path_text.c_str()}) ==
            ExitCode::unexpected_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("cannot reserve a temporary report path") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(target_path));
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze output disambiguates duplicate translation-unit observations") {
    const auto compile_commands = fixture_path("m2/duplicate_tu_entries/compile_commands.json");

    REQUIRE(run({"analyze", "--compile-commands", compile_commands.c_str(), "--top", "3"}) ==
            ExitCode::success);
    CHECK(out.str().find("src/app/main.cpp [directory: build/debug]") != std::string::npos);
    CHECK(out.str().find("src/app/main.cpp [directory: build/release]") != std::string::npos);
    CHECK(out.str().find("include/common/config.h (affected translation units: 3)") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact output keeps duplicate translation-unit observations") {
    const auto compile_commands = fixture_path("m2/duplicate_tu_entries/compile_commands.json");

    REQUIRE(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
                 "src/app/main.cpp"}) == ExitCode::success);
    CHECK(out.str().find("affected translation units: 2") != std::string::npos);
    CHECK(out.str().find("src/app/main.cpp [directory: build/debug] [direct]") !=
          std::string::npos);
    CHECK(out.str().find("src/app/main.cpp [directory: build/release] [direct]") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "analyze without any input source returns exit 2") {
    CHECK(run({"analyze"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("at least one input source") != std::string::npos);
    CHECK(err.str().find("--compile-commands") != std::string::npos);
    CHECK(err.str().find("--cmake-file-api") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact without any input source returns exit 2") {
    CHECK(run({"impact", "--changed-file", "src/main.cpp"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("at least one input source") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "missing impact changed-file returns exit 2") {
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str()}) ==
          ExitCode::cli_usage_error);
}

TEST_CASE_FIXTURE(CliFixture, "unknown report format value returns exit 2") {
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
               "yaml"}) == ExitCode::cli_usage_error);
}

TEST_CASE_FIXTURE(CliFixture, "html analyze format is implemented and emits HTML") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
                 "html", "--top", "2"}) == ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("<!doctype html>") != std::string::npos);
    CHECK(out.str().find("data-report-type=\"analyze\"") != std::string::npos);
    CHECK(out.str().find("recognized but not implemented") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "html impact format is implemented and emits HTML") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--format", "html"}) ==
            ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("<!doctype html>") != std::string::npos);
    CHECK(out.str().find("data-report-type=\"impact\"") != std::string::npos);
    CHECK(out.str().find("recognized but not implemented") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "json analyze format is implemented and emits JSON") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
                 "json", "--top", "2"}) == ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("\"format\": \"cmake-xray.analysis\"") != std::string::npos);
    CHECK(out.str().find("recognized but not implemented") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "json impact format is implemented and emits JSON") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--format", "json"}) ==
            ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("\"format\": \"cmake-xray.impact\"") != std::string::npos);
    CHECK(out.str().find("\"changed_file_source\": \"compile_database_directory\"") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "json analyze --output writes the file with empty streams") {
    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "report.json";
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
                 "json", "--output", target.string().c_str()}) == ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    REQUIRE(std::filesystem::is_regular_file(target));
    const auto contents = read_text_file(target);
    CHECK(contents.find("\"format\": \"cmake-xray.analysis\"") != std::string::npos);
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE_FIXTURE(CliFixture, "json impact --output writes the file with empty streams") {
    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "impact.json";
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--format", "json", "--output",
                 target.string().c_str()}) == ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    REQUIRE(std::filesystem::is_regular_file(target));
    const auto contents = read_text_file(target);
    CHECK(contents.find("\"format\": \"cmake-xray.impact\"") != std::string::npos);
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE_FIXTURE(CliFixture, "json analyze with non-existent compile-commands emits text error") {
    const TemporaryDirectory temp_dir;
    const auto missing = (temp_dir.path() / "no-such.json").string();
    CHECK(run({"analyze", "--compile-commands", missing.c_str(), "--format", "json"}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    // Errors stay text-on-stderr in JSON v1; no JSON error object is emitted.
    CHECK(err.str().find("\"format\":") == std::string::npos);
    CHECK(err.str().find("\"error\":") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "json analyze with invalid file api reply emits text error") {
    const TemporaryDirectory temp_dir;
    const auto bad = (temp_dir.path() / "no-reply").string();
    CHECK(run({"analyze", "--cmake-file-api", bad.c_str(), "--format", "json"}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("\"format\":") == std::string::npos);
    CHECK(err.str().find("\"error\":") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "json analyze with invalid compile-commands JSON emits text error") {
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("invalid_syntax/compile_commands.json").c_str(), "--format",
               "json"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    // No JSON error object: stderr stays plain text with no JSON-shaped fields.
    CHECK(err.str().find("\"format\":") == std::string::npos);
    CHECK(err.str().find("\"error\":") == std::string::npos);
    CHECK(err.str().find("\"format_version\":") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "dot analyze format is implemented and emits DOT") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
                 "dot", "--top", "2"}) == ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("digraph cmake_xray_analysis {") != std::string::npos);
    CHECK(out.str().find("xray_report_type=\"analyze\"") != std::string::npos);
    CHECK(out.str().find("recognized but not implemented") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "dot impact format is implemented and emits DOT") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--format", "dot"}) ==
            ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("digraph cmake_xray_impact {") != std::string::npos);
    CHECK(out.str().find("xray_report_type=\"impact\"") != std::string::npos);
}

TEST_CASE("dot impact refuses to render when changed_file_source is unresolved_file_api_source_root") {
    // Build an ImpactResult that mimics the unresolved-file-api state and run
    // the CLI through the stub-port pipeline so the dot path is reached.
    ImpactResult unresolved_result{};
    unresolved_result.application = xray::hexagon::model::application_info();
    unresolved_result.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    unresolved_result.inputs.changed_file = std::string{"src/missing.cpp"};
    unresolved_result.inputs.changed_file_source =
        xray::hexagon::model::ChangedFileSource::unresolved_file_api_source_root;

    class FixedImpactPort final : public xray::hexagon::ports::driving::AnalyzeImpactPort {
    public:
        explicit FixedImpactPort(ImpactResult result) : result_(std::move(result)) {}
        ImpactResult analyze_impact(
            xray::hexagon::ports::driving::AnalyzeImpactRequest /*request*/) const override {
            return result_;
        }

    private:
        ImpactResult result_;
    };

    const StubAnalyzeProjectPort analyze_project_port{AnalysisResult{}};
    const FixedImpactPort impact_port{unresolved_result};
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const StubGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, impact_port, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",     "impact", "--cmake-file-api",
                          "/tmp/empty",     "--changed-file", "src/missing.cpp",
                          "--format",       "dot"};

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("file-api source root is unresolved") != std::string::npos);
    // No DOT graph should leak into stderr either.
    CHECK(err.str().find("digraph cmake_xray_impact") == std::string::npos);
}

// AP M5-1.4 Tranche C.2 Impact-Negativfall-Matrix line 551:
// changed_file_source=unresolved_file_api_source_root must surface as a text
// error on stderr with a non-zero exit and no HTML document. Mirrors the DOT
// equivalent above so HTML respects the same render-precondition contract.
TEST_CASE("html impact refuses to render when changed_file_source is unresolved_file_api_source_root") {
    ImpactResult unresolved_result{};
    unresolved_result.application = xray::hexagon::model::application_info();
    unresolved_result.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    unresolved_result.inputs.changed_file = std::string{"src/missing.cpp"};
    unresolved_result.inputs.changed_file_source =
        xray::hexagon::model::ChangedFileSource::unresolved_file_api_source_root;

    class FixedImpactPort final : public xray::hexagon::ports::driving::AnalyzeImpactPort {
    public:
        explicit FixedImpactPort(ImpactResult result) : result_(std::move(result)) {}
        ImpactResult analyze_impact(
            xray::hexagon::ports::driving::AnalyzeImpactRequest /*request*/) const override {
            return result_;
        }

    private:
        ImpactResult result_;
    };

    const StubAnalyzeProjectPort analyze_project_port{AnalysisResult{}};
    const FixedImpactPort impact_port{unresolved_result};
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const StubGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, impact_port, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",     "impact",         "--cmake-file-api",
                          "/tmp/empty",     "--changed-file", "src/missing.cpp",
                          "--format",       "html"};

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("file-api source root is unresolved") != std::string::npos);
    CHECK(err.str().find("--format html") != std::string::npos);
    // No HTML document should leak into stderr either.
    CHECK(err.str().find("<!doctype html>") == std::string::npos);
    CHECK(err.str().find("<html") == std::string::npos);
}

// AP M5-1.4 Tranche C.2 Impact-Negativfall-Matrix line 550: an ImpactResult
// whose changed_file is std::nullopt must not produce HTML. The CLI usage
// validator rejects --changed-file=missing before analysis, so this exercises
// the post-analysis precondition path via a stub ImpactPort that returns the
// unset optional. Without the precondition the HTML adapter would emit "not
// provided" placeholders; the precondition keeps that document from leaving
// the binary.
TEST_CASE("html impact refuses to render when changed_file is std::nullopt") {
    ImpactResult unset_result{};
    unset_result.application = xray::hexagon::model::application_info();
    unset_result.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    // Leave inputs.changed_file as std::nullopt deliberately.

    class FixedImpactPort final : public xray::hexagon::ports::driving::AnalyzeImpactPort {
    public:
        explicit FixedImpactPort(ImpactResult result) : result_(std::move(result)) {}
        ImpactResult analyze_impact(
            xray::hexagon::ports::driving::AnalyzeImpactRequest /*request*/) const override {
            return result_;
        }

    private:
        ImpactResult result_;
    };

    const StubAnalyzeProjectPort analyze_project_port{AnalysisResult{}};
    const FixedImpactPort impact_port{unset_result};
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const StubGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, impact_port, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "impact",         "--cmake-file-api",
                          "/tmp/empty", "--changed-file", "src/whatever.cpp",
                          "--format",   "html"};

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("changed_file is missing") != std::string::npos);
    // No partial HTML document with "not provided" placeholders must leak.
    CHECK(err.str().find("<!doctype html>") == std::string::npos);
    CHECK(err.str().find("not provided") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "dot impact analyze emits valid DOT and empty stderr") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--format", "dot"}) ==
            ExitCode::success);
    CHECK(err.str().empty());
    CHECK(out.str().find("digraph cmake_xray_impact {") != std::string::npos);
    CHECK(out.str().find("changed_file [") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "dot impact --output writes the file with empty streams") {
    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "impact.dot";
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--format", "dot", "--output",
                 target.string().c_str()}) == ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    REQUIRE(std::filesystem::is_regular_file(target));
    const auto contents = read_text_file(target);
    CHECK(contents.find("digraph cmake_xray_impact {") != std::string::npos);
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE_FIXTURE(CliFixture, "dot analyze with non-existent compile-commands emits text error") {
    const TemporaryDirectory temp_dir;
    const auto missing = (temp_dir.path() / "no-such.json").string();
    CHECK(run({"analyze", "--compile-commands", missing.c_str(), "--format", "dot"}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    // No DOT graph leaks into either stream.
    CHECK(err.str().find("digraph") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "dot analyze with invalid compile-commands JSON emits text error") {
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("invalid_syntax/compile_commands.json").c_str(), "--format",
               "dot"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("digraph") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "dot analyze with --top 0 is rejected by the existing PositiveNumber check") {
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
               "dot", "--top", "0"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("digraph") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "dot impact rejects --top because impact has no top semantics") {
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(),
               "--changed-file", "include/common/shared.h", "--format", "dot", "--top",
               "5"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "dot impact without --changed-file returns the changed-file required error") {
    CHECK(run({"impact", "--cmake-file-api",
               fixture_path("m4/file_api_only/build").c_str(), "--format", "dot"}) !=
          ExitCode::success);
    CHECK(err.str().find("impact requires --changed-file") != std::string::npos);
    CHECK(out.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "dot analyze --output writes the file with empty streams") {
    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "report.dot";
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
                 "dot", "--output", target.string().c_str()}) == ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    REQUIRE(std::filesystem::is_regular_file(target));
    const auto contents = read_text_file(target);
    CHECK(contents.find("digraph cmake_xray_analysis {") != std::string::npos);
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE_FIXTURE(CliFixture, "html --output writes to the file and leaves stdout/stderr empty") {
    const TemporaryDirectory temp_dir;
    const auto target_path = (temp_dir.path() / "report.html").string();

    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
                 "html", "--output", target_path.c_str(), "--top", "2"}) == ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    REQUIRE(std::filesystem::exists(target_path));
    std::ifstream file{target_path};
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    CHECK(content.find("<!doctype html>") != std::string::npos);
    CHECK(content.find("data-report-type=\"analyze\"") != std::string::npos);
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

// AP M5-1.4 Tranche C.2: lock the html impact --output stdout/stderr/atomic
// contract in a CLI test so the impact path matches the analyze path covered
// above. Mirrors json impact --output and dot impact --output.
TEST_CASE_FIXTURE(CliFixture, "html impact --output writes the file with empty streams") {
    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "impact.html";
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--format", "html", "--output",
                 target.string().c_str()}) == ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    REQUIRE(std::filesystem::is_regular_file(target));
    const auto contents = read_text_file(target);
    CHECK(contents.find("<!doctype html>") != std::string::npos);
    CHECK(contents.find("data-report-type=\"impact\"") != std::string::npos);
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

// AP M5-1.4 Tranche C.2 Impact-Negativfall-Matrix: missing compile-commands
// reaches the HTML pipeline only as a text error on stderr. No HTML error
// document is emitted, stdout stays empty, and the binary exits non-zero.
TEST_CASE_FIXTURE(CliFixture,
                  "html analyze with non-existent compile-commands emits text error") {
    const TemporaryDirectory temp_dir;
    const auto missing = (temp_dir.path() / "no-such.json").string();
    CHECK(run({"analyze", "--compile-commands", missing.c_str(), "--format", "html"}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    // No HTML document leaks into either stream.
    CHECK(err.str().find("<!doctype html>") == std::string::npos);
    CHECK(err.str().find("<html") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "html impact with non-existent compile-commands emits text error") {
    const TemporaryDirectory temp_dir;
    const auto missing = (temp_dir.path() / "no-such.json").string();
    CHECK(run({"impact", "--compile-commands", missing.c_str(), "--changed-file",
               "include/common/shared.h", "--format", "html"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("<!doctype html>") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "html analyze with invalid compile-commands JSON emits text error") {
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("invalid_syntax/compile_commands.json").c_str(), "--format",
               "html"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("<!doctype html>") == std::string::npos);
    CHECK(err.str().find("<html") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "html analyze with invalid file api reply emits text error") {
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m4/invalid_file_api/compile_commands.json").c_str(),
               "--cmake-file-api", fixture_path("m4/invalid_file_api/build").c_str(),
               "--format", "html"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("<!doctype html>") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "html analyze with non-existent file api reply emits text error") {
    const TemporaryDirectory temp_dir;
    const auto bad = (temp_dir.path() / "no-reply").string();
    CHECK(run({"analyze", "--cmake-file-api", bad.c_str(), "--format", "html"}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("<!doctype html>") == std::string::npos);
}

// --top 0 must be rejected by the existing PositiveNumber CLI validator before
// the HTML pipeline ever runs; nothing HTML-shaped must appear on stdout or
// stderr. Mirrors dot/markdown coverage for the same rule.
TEST_CASE_FIXTURE(CliFixture,
                  "html analyze with --top 0 is rejected and emits no HTML") {
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
               "html", "--top", "0"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("<!doctype html>") == std::string::npos);
    CHECK(err.str().find("<html") == std::string::npos);
}

// AP M5-1.4 leaves `impact --top` outside the M5 contract. The CLI usage
// validator must keep rejecting --top for impact even when --format html is
// requested; no HTML output must reach stdout.
TEST_CASE_FIXTURE(CliFixture,
                  "html impact rejects --top because impact has no top semantics") {
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(),
               "--changed-file", "include/common/shared.h", "--format", "html", "--top",
               "5"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("<!doctype html>") == std::string::npos);
}

// Write fault: the atomic writer surfaces a non-existent intermediate
// directory as an unexpected_error with a clear hint. Mirrors the markdown
// write fault test; ensures HTML respects the same atomic-writer contract.
TEST_CASE_FIXTURE(CliFixture,
                  "html analyze --output write failures return exit 1 without leaving temp files") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "report.html").string();

    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
               "html", "--output", missing_target.c_str(), "--top", "2"}) ==
          ExitCode::unexpected_error);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("cannot write report") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(missing_target));
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}


TEST_CASE_FIXTURE(CliFixture,
                  "impact json without --changed-file returns the changed-file required error") {
    CHECK(run({"impact", "--cmake-file-api",
               fixture_path("m4/file_api_only/build").c_str(), "--format", "json"}) ==
          ExitCode::cli_usage_error);
    CHECK(err.str().find("impact requires --changed-file") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact html without --changed-file returns the changed-file required error") {
    CHECK(run({"impact", "--cmake-file-api",
               fixture_path("m4/file_api_only/build").c_str(), "--format", "html"}) ==
          ExitCode::cli_usage_error);
    CHECK(err.str().find("impact requires --changed-file") != std::string::npos);
    CHECK(err.str().find("recognized but not implemented") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact missing --changed-file with markdown returns exit 2") {
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
               "markdown"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("impact requires --changed-file") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "analyze --help mentions all five format values") {
    REQUIRE(run({"analyze", "--help"}) == ExitCode::success);
    for (const auto* value : {"console", "markdown", "html", "json", "dot"}) {
        CHECK(out.str().find(value) != std::string::npos);
    }
}

TEST_CASE_FIXTURE(CliFixture, "nonexistent file returns exit 3") {
    CHECK(run({"analyze", "--compile-commands", "/nonexistent/compile_commands.json"}) ==
          ExitCode::input_not_accessible);
    CHECK(err.str().find("cannot open") != std::string::npos);
    CHECK(err.str().find("hint:") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "invalid JSON returns exit 4 for analyze") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "invalid_syntax/compile_commands.json").c_str()}) ==
          ExitCode::input_invalid);
    CHECK(err.str().find("not valid JSON") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "invalid JSON returns exit 4 for impact") {
    CHECK(run({"impact", "--compile-commands",
               (testdata + "invalid_syntax/compile_commands.json").c_str(), "--changed-file",
               "src/main.cpp"}) == ExitCode::input_invalid);
    CHECK(err.str().find("not valid JSON") != std::string::npos);
}

#ifndef _WIN32
TEST_CASE_FIXTURE(CliFixture, "write fault during report generation returns exit 1") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    const TemporaryDirectory temp_dir;
    const auto report_path = temp_dir.path() / "report.md";
    const auto report_path_text = report_path.string();

    struct rlimit old_limit{};
    getrlimit(RLIMIT_FSIZE, &old_limit);
    const struct rlimit zero_limit = {0, old_limit.rlim_max};
    setrlimit(RLIMIT_FSIZE, &zero_limit);
    std::signal(SIGXFSZ, SIG_IGN);

    const auto exit_code =
        run({"analyze", "--compile-commands", compile_commands.c_str(), "--format", "markdown",
             "--output", report_path_text.c_str(), "--top", "2"});

    setrlimit(RLIMIT_FSIZE, &old_limit);
    std::signal(SIGXFSZ, SIG_DFL);

    CHECK(exit_code == ExitCode::unexpected_error);
    CHECK(err.str().find("error: cannot write report:") != std::string::npos);
}
#endif

TEST_CASE("invalid entries report is truncated after 20 diagnostics") {
    std::vector<EntryDiagnostic> diagnostics;
    diagnostics.reserve(20);
    for (std::size_t index = 0; index < 20; ++index) {
        diagnostics.emplace_back(index, "missing field");
    }

    const StubAnalyzeProjectPort analyze_project_port{
        [&] {
            AnalysisResult result;
            result.application = xray::hexagon::model::application_info();
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::invalid_entries,
                "compile_commands.json contains 23 invalid entries: /tmp/compile_commands.json",
                {},
                diagnostics,
                23,
            };
            return result;
        }(),
    };
    const StubAnalyzeImpactPort analyze_impact_port;
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const StubGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, analyze_impact_port, report_ports};
    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "/tmp/compile_commands.json"};

    CHECK(cli.run(4, argv, out, err) == ExitCode::input_invalid);
    CHECK(err.str().find("entry 19") != std::string::npos);
    CHECK(err.str().find("... and 3 more invalid entries") != std::string::npos);
}

TEST_CASE("unexpected compile database errors map to exit code 1") {
    const StubAnalyzeProjectPort analyze_project_port{
        [] {
            AnalysisResult result;
            result.application = xray::hexagon::model::application_info();
            result.compile_database = CompileDatabaseResult{
                static_cast<CompileDatabaseError>(999),
                "unexpected compile database failure",
                {},
                {},
            };
            return result;
        }(),
    };
    const StubAnalyzeImpactPort analyze_impact_port;
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const StubGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, analyze_impact_port, report_ports};
    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "/tmp/compile_commands.json"};

    CHECK(cli.run(4, argv, out, err) == ExitCode::unexpected_error);
    CHECK(err.str().find("unexpected compile database failure") != std::string::npos);
}

TEST_CASE("file api not accessible maps to exit code 3 with file api hint") {
    const StubAnalyzeProjectPort analyze_project_port{
        [] {
            AnalysisResult result;
            result.application = xray::hexagon::model::application_info();
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::file_api_not_accessible,
                "cannot access cmake file api reply directory: /nonexistent",
                {},
                {},
            };
            return result;
        }(),
    };
    const StubAnalyzeImpactPort analyze_impact_port;
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const StubGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, analyze_impact_port, report_ports};
    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "/tmp/compile_commands.json"};

    CHECK(cli.run(4, argv, out, err) == ExitCode::input_not_accessible);
    CHECK(err.str().find("cmake file api") != std::string::npos);
    CHECK(err.str().find("hint:") != std::string::npos);
    CHECK(err.str().find("--cmake-file-api") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "file api only analyze runs against real m4 fixture") {
    const auto file_api_path = fixture_path("m4/file_api_only/build");

    REQUIRE(run({"analyze", "--cmake-file-api", file_api_path.c_str()}) == ExitCode::success);
    CHECK(out.str().find("observation source: derived") != std::string::npos);
    CHECK(out.str().find("target metadata: loaded") != std::string::npos);
    CHECK(out.str().find("translation unit ranking") != std::string::npos);
    CHECK(out.str().find("main.cpp") != std::string::npos);
    CHECK(out.str().find("core.cpp") != std::string::npos);
    CHECK(out.str().find("[targets:") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "file api only analyze accepts reply directory directly") {
    const auto reply_path = fixture_path("m4/file_api_only/build/.cmake/api/v1/reply");

    REQUIRE(run({"analyze", "--cmake-file-api", reply_path.c_str()}) == ExitCode::success);
    CHECK(out.str().find("translation unit ranking") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "file api only impact resolves changed file via source root") {
    const auto file_api_path = fixture_path("m4/file_api_only/build");

    REQUIRE(run({"impact", "--cmake-file-api", file_api_path.c_str(), "--changed-file",
                 "src/main.cpp"}) == ExitCode::success);
    CHECK(out.str().find("impact analysis") != std::string::npos);
    CHECK(out.str().find("main.cpp") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "file api only impact accepts reply directory directly") {
    const auto reply_path = fixture_path("m4/file_api_only/build/.cmake/api/v1/reply");

    REQUIRE(run({"impact", "--cmake-file-api", reply_path.c_str(), "--changed-file",
                 "src/main.cpp"}) == ExitCode::success);
    CHECK(out.str().find("impact analysis") != std::string::npos);
    CHECK(out.str().find("main.cpp") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "mixed path analyze enriches compile database with file api targets") {
    const auto compile_commands = fixture_path("m4/partial_targets/compile_commands.json");
    const auto file_api_path = fixture_path("m4/partial_targets/build");

    REQUIRE(run({"analyze", "--compile-commands", compile_commands.c_str(), "--cmake-file-api",
                 file_api_path.c_str()}) == ExitCode::success);
    CHECK(out.str().find("observation source: exact") != std::string::npos);
    CHECK(out.str().find("target metadata: partial") != std::string::npos);
    CHECK(out.str().find("translation unit ranking") != std::string::npos);
    CHECK(out.str().find("[targets: app]") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "mixed path impact enriches compile database with file api targets") {
    const auto compile_commands = fixture_path("m4/partial_targets/compile_commands.json");
    const auto file_api_path = fixture_path("m4/partial_targets/build");

    REQUIRE(run({"impact", "--compile-commands", compile_commands.c_str(), "--cmake-file-api",
                 file_api_path.c_str(), "--changed-file", "src/main.cpp"}) == ExitCode::success);
    CHECK(out.str().find("impact analysis") != std::string::npos);
    CHECK(out.str().find("observation source: exact") != std::string::npos);
    CHECK(out.str().find("affected targets:") != std::string::npos);
    CHECK(out.str().find("directly affected targets") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "mixed path impact hits target with absolute changed file") {
    const auto compile_commands = fixture_path("m4/partial_targets/compile_commands.json");
    const auto file_api_path = fixture_path("m4/partial_targets/build");

    REQUIRE(run({"impact", "--compile-commands", compile_commands.c_str(), "--cmake-file-api",
                 file_api_path.c_str(), "--changed-file", "/project/src/main.cpp"}) ==
            ExitCode::success);
    CHECK(out.str().find("affected translation units: 1") != std::string::npos);
    CHECK(out.str().find("observation source: exact") != std::string::npos);
    CHECK(out.str().find("target metadata: partial") != std::string::npos);
    CHECK(out.str().find("affected targets: 1") != std::string::npos);
    CHECK(out.str().find("[targets: app]") != std::string::npos);
    CHECK(out.str().find("[direct]") != std::string::npos);
    CHECK(out.str().find("app [type: EXECUTABLE]") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "multi target analyze shows shared source targets") {
    const auto file_api_path = fixture_path("m4/multi_target/build");

    REQUIRE(run({"analyze", "--cmake-file-api", file_api_path.c_str()}) == ExitCode::success);
    CHECK(out.str().find("shared.cpp") != std::string::npos);
    CHECK(out.str().find("[targets: app, core]") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "nonexistent file api path returns exit 3") {
    CHECK(run({"analyze", "--cmake-file-api", "/nonexistent/reply"}) ==
          ExitCode::input_not_accessible);
    CHECK(err.str().find("cmake file api") != std::string::npos);
    CHECK(err.str().find("hint:") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "invalid file api in mixed path returns exit 4") {
    const auto compile_commands = fixture_path("m4/invalid_file_api/compile_commands.json");
    const auto file_api_path = fixture_path("m4/invalid_file_api/build");

    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--cmake-file-api",
               file_api_path.c_str()}) == ExitCode::input_invalid);
    CHECK(err.str().find("cmake file api") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "empty reply directory returns exit 4") {
    const auto file_api_path = fixture_path("m4/empty_reply/build");

    CHECK(run({"analyze", "--cmake-file-api", file_api_path.c_str()}) ==
          ExitCode::input_invalid);
}

TEST_CASE_FIXTURE(CliFixture, "m3 output remains unchanged without file api") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");

    REQUIRE(run({"analyze", "--compile-commands", compile_commands.c_str(), "--top", "3"}) ==
            ExitCode::success);
    // M3 output must not contain target or observation source metadata
    CHECK(out.str().find("observation source") == std::string::npos);
    CHECK(out.str().find("target metadata") == std::string::npos);
    CHECK(err.str().empty());
}

namespace {

class FailingRenderer final : public xray::adapters::cli::CliReportRenderer {
public:
    explicit FailingRenderer(std::string message) : message_(std::move(message)) {}

    xray::adapters::cli::RenderResult render() const override {
        return {std::nullopt, xray::adapters::cli::RenderError{message_}};
    }

private:
    std::string message_;
};

class ThrowingGenerateReportPort final : public xray::hexagon::ports::driving::GenerateReportPort {
public:
    std::string generate_analysis_report(const AnalysisResult&,
                                         std::size_t) const override {
        throw std::runtime_error("analysis report exception");
    }

    std::string generate_impact_report(const ImpactResult&) const override {
        throw std::runtime_error("impact report exception");
    }
};

class SuccessRenderer final : public xray::adapters::cli::CliReportRenderer {
public:
    explicit SuccessRenderer(std::string content) : content_(std::move(content)) {}

    xray::adapters::cli::RenderResult render() const override {
        return {content_, std::nullopt};
    }

private:
    std::string content_;
};

}  // namespace

TEST_CASE("analysis cli report renderer maps thrown exceptions into a RenderError") {
    const ThrowingGenerateReportPort port;
    AnalysisResult result;
    result.application = xray::hexagon::model::application_info();
    const xray::adapters::cli::AnalysisCliReportRenderer renderer{port, result, 1};

    const auto rendered = renderer.render();

    CHECK_FALSE(rendered.content.has_value());
    REQUIRE(rendered.error.has_value());
    CHECK(rendered.error->message.find("analysis report exception") != std::string::npos);
}

TEST_CASE("impact cli report renderer maps thrown exceptions into a RenderError") {
    const ThrowingGenerateReportPort port;
    ImpactResult result;
    result.application = xray::hexagon::model::application_info();
    const xray::adapters::cli::ImpactCliReportRenderer renderer{port, result};

    const auto rendered = renderer.render();

    CHECK_FALSE(rendered.content.has_value());
    REQUIRE(rendered.error.has_value());
    CHECK(rendered.error->message.find("impact report exception") != std::string::npos);
}

TEST_CASE("emit_rendered_report keeps an existing file when the renderer reports an error") {
    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "report.md";
    {
        std::ofstream existing(target);
        existing << "untouched";
    }

    const FailingRenderer renderer{"simulated rendering failed"};
    xray::adapters::cli::DefaultAtomicFilePlatformOps ops;
    xray::adapters::cli::AtomicReportWriter writer{ops};

    std::ostringstream out;
    std::ostringstream err;
    const auto exit_code = xray::adapters::cli::emit_rendered_report(
        renderer, target.string(), writer, xray::adapters::cli::CliOutputStreams{out, err});

    CHECK(exit_code == ExitCode::unexpected_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("cannot render report") != std::string::npos);
    CHECK(err.str().find("simulated rendering failed") != std::string::npos);
    CHECK(read_text_file(target) == "untouched");
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE("emit_rendered_report writes nothing to stdout when the renderer reports an error") {
    const FailingRenderer renderer{"render error path"};
    xray::adapters::cli::DefaultAtomicFilePlatformOps ops;
    xray::adapters::cli::AtomicReportWriter writer{ops};

    std::ostringstream out;
    std::ostringstream err;
    const auto exit_code =
        xray::adapters::cli::emit_rendered_report(renderer, "", writer, xray::adapters::cli::CliOutputStreams{out, err});

    CHECK(exit_code == ExitCode::unexpected_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("render error path") != std::string::npos);
}

TEST_CASE("json --output keeps the existing target file when the JSON renderer throws") {
    // Plan AP 1.2 Tranche C step 4: a CliReportRenderer doppelgaenger drives
    // the JSON write path; the existing target file must stay untouched, no
    // partial JSON appears on stdout, and the CLI exits with a non-zero code
    // and a text error on stderr.
    AnalysisResult success_result;
    success_result.application = xray::hexagon::model::application_info();
    success_result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    const StubAnalyzeProjectPort analyze_project_port{success_result};
    const StubAnalyzeImpactPort analyze_impact_port;
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const ThrowingGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const StubGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, analyze_impact_port, report_ports};

    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "report.json";
    {
        std::ofstream existing(target);
        existing << "{\"untouched\": true}";
    }

    std::ostringstream out;
    std::ostringstream err;
    const auto target_path = target.string();
    const char* argv[] = {"cmake-xray",
                          "analyze",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--format",
                          "json",
                          "--output",
                          target_path.c_str()};
    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("cannot render report") != std::string::npos);
    CHECK(err.str().find("analysis report exception") != std::string::npos);
    CHECK(read_text_file(target) == "{\"untouched\": true}");
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

// AP M5-1.4 Tranche C.2: HTML render-error doppelgaenger for analyze. Drives
// the HTML --output path through a throwing GenerateReportPort so the CLI
// must surface a non-zero exit, a text error on stderr, no partial HTML on
// stdout, and an unchanged target file. Mirrors the json/dot doppelgaenger
// tests above.
TEST_CASE("html analyze --output keeps the existing target file when the HTML renderer throws") {
    AnalysisResult success_result;
    success_result.application = xray::hexagon::model::application_info();
    success_result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    const StubAnalyzeProjectPort analyze_project_port{success_result};
    const StubAnalyzeImpactPort analyze_impact_port;
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const ThrowingGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, analyze_impact_port, report_ports};

    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "report.html";
    {
        std::ofstream existing(target);
        existing << "<!doctype html><html>untouched</html>";
    }

    std::ostringstream out;
    std::ostringstream err;
    const auto target_path = target.string();
    const char* argv[] = {"cmake-xray",
                          "analyze",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--format",
                          "html",
                          "--output",
                          target_path.c_str()};
    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("cannot render report") != std::string::npos);
    CHECK(err.str().find("analysis report exception") != std::string::npos);
    CHECK(read_text_file(target) == "<!doctype html><html>untouched</html>");
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

// AP M5-1.4 Tranche C.2: HTML render-error doppelgaenger for impact. The
// impact-renderer path must respect the same render-error contract as the
// analyze path: non-zero exit, text error on stderr, no HTML on stdout,
// existing target file unchanged.
TEST_CASE("html impact --output keeps the existing target file when the HTML renderer throws") {
    AnalysisResult unused_analysis;
    unused_analysis.application = xray::hexagon::model::application_info();
    unused_analysis.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    ImpactResult impact_result;
    impact_result.application = xray::hexagon::model::application_info();
    impact_result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    impact_result.inputs.changed_file = std::string{"src/app/main.cpp"};
    impact_result.inputs.changed_file_source =
        xray::hexagon::model::ChangedFileSource::compile_database_directory;

    class FixedImpactPort final : public xray::hexagon::ports::driving::AnalyzeImpactPort {
    public:
        explicit FixedImpactPort(ImpactResult result) : result_(std::move(result)) {}
        ImpactResult analyze_impact(
            xray::hexagon::ports::driving::AnalyzeImpactRequest /*request*/) const override {
            return result_;
        }

    private:
        ImpactResult result_;
    };

    const StubAnalyzeProjectPort analyze_project_port{unused_analysis};
    const FixedImpactPort impact_port{impact_result};
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const ThrowingGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, impact_port, report_ports};

    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "impact.html";
    {
        std::ofstream existing(target);
        existing << "<!doctype html><html>impact untouched</html>";
    }

    std::ostringstream out;
    std::ostringstream err;
    const auto target_path = target.string();
    const char* argv[] = {"cmake-xray",
                          "impact",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--changed-file",
                          "src/app/main.cpp",
                          "--format",
                          "html",
                          "--output",
                          target_path.c_str()};
    const int exit_code = cli.run(10, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("cannot render report") != std::string::npos);
    CHECK(err.str().find("impact report exception") != std::string::npos);
    CHECK(read_text_file(target) == "<!doctype html><html>impact untouched</html>");
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE("dot --output keeps the existing target file when the DOT renderer throws") {
    AnalysisResult success_result;
    success_result.application = xray::hexagon::model::application_info();
    success_result.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    const StubAnalyzeProjectPort analyze_project_port{success_result};
    const StubAnalyzeImpactPort analyze_impact_port;
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const ThrowingGenerateReportPort dot_report_port;
    const StubGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, analyze_impact_port, report_ports};

    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "report.dot";
    {
        std::ofstream existing(target);
        existing << "digraph untouched { foo; }";
    }

    std::ostringstream out;
    std::ostringstream err;
    const auto target_path = target.string();
    const char* argv[] = {"cmake-xray",
                          "analyze",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--format",
                          "dot",
                          "--output",
                          target_path.c_str()};
    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("cannot render report") != std::string::npos);
    CHECK(read_text_file(target) == "digraph untouched { foo; }");
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE("emit_rendered_report streams content to stdout when output_path is empty") {
    const SuccessRenderer renderer{"streamed body"};
    xray::adapters::cli::DefaultAtomicFilePlatformOps ops;
    xray::adapters::cli::AtomicReportWriter writer{ops};

    std::ostringstream out;
    std::ostringstream err;
    const auto exit_code =
        xray::adapters::cli::emit_rendered_report(renderer, "", writer, xray::adapters::cli::CliOutputStreams{out, err});

    CHECK(exit_code == ExitCode::success);
    CHECK(out.str() == "streamed body");
    CHECK(err.str().empty());
}

TEST_CASE("emit_rendered_report writes the content atomically to the target path") {
    const TemporaryDirectory temp_dir;
    const auto target = temp_dir.path() / "report.md";
    const SuccessRenderer renderer{"file body"};
    xray::adapters::cli::DefaultAtomicFilePlatformOps ops;
    xray::adapters::cli::AtomicReportWriter writer{ops};

    std::ostringstream out;
    std::ostringstream err;
    const auto exit_code = xray::adapters::cli::emit_rendered_report(
        renderer, target.string(), writer, xray::adapters::cli::CliOutputStreams{out, err});

    CHECK(exit_code == ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    CHECK(read_text_file(target) == "file body");
    CHECK_FALSE(contains_temporary_report_file(temp_dir.path()));
}

TEST_CASE("emit_rendered_report surfaces atomic writer failures with a write hint") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = temp_dir.path() / "missing-dir" / "report.md";
    const SuccessRenderer renderer{"will not land"};
    xray::adapters::cli::DefaultAtomicFilePlatformOps ops;
    xray::adapters::cli::AtomicReportWriter writer{ops};

    std::ostringstream out;
    std::ostringstream err;
    const auto exit_code = xray::adapters::cli::emit_rendered_report(
        renderer, missing_target.string(), writer, xray::adapters::cli::CliOutputStreams{out, err});

    CHECK(exit_code == ExitCode::unexpected_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("cannot write report") != std::string::npos);
    CHECK(err.str().find("hint: check the output path") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(missing_target));
}

TEST_CASE("file api invalid maps to exit code 4 with file api hint") {
    const StubAnalyzeProjectPort analyze_project_port{
        [] {
            AnalysisResult result;
            result.application = xray::hexagon::model::application_info();
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::file_api_invalid,
                "cmake file api codemodel is not valid JSON: /tmp/reply/codemodel.json",
                {},
                {},
            };
            return result;
        }(),
    };
    const StubAnalyzeImpactPort analyze_impact_port;
    const StubGenerateReportPort console_report_port;
    const StubGenerateReportPort markdown_report_port;
    const StubGenerateReportPort json_report_port;
    const StubGenerateReportPort dot_report_port;
    const StubGenerateReportPort html_report_port;
    const xray::adapters::cli::ReportPorts report_ports{console_report_port,
                                                        markdown_report_port,
                                                        json_report_port,
                                                        dot_report_port,
                                                        html_report_port};
    const CliAdapter cli{analyze_project_port, analyze_impact_port, report_ports};
    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "/tmp/compile_commands.json"};

    CHECK(cli.run(4, argv, out, err) == ExitCode::input_invalid);
    CHECK(err.str().find("cmake file api") != std::string::npos);
    CHECK(err.str().find("hint:") != std::string::npos);
    CHECK(err.str().find("reply data") != std::string::npos);
}
