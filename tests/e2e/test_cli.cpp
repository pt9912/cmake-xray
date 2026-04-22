#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

#include "adapters/cli/cli_adapter.h"
#include "adapters/cli/exit_codes.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "adapters/input/source_parsing_include_adapter.h"
#include "adapters/output/console_report_adapter.h"
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
using xray::adapters::input::CompileCommandsJsonAdapter;
using xray::adapters::input::SourceParsingIncludeAdapter;
using xray::adapters::output::ConsoleReportAdapter;
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
    SourceParsingIncludeAdapter include_resolver_adapter;
    ConsoleReportAdapter report_adapter;
    ProjectAnalyzer project_analyzer{compile_database_adapter, include_resolver_adapter};
    ImpactAnalyzer impact_analyzer{compile_database_adapter, include_resolver_adapter};
    ReportGenerator report_generator{report_adapter};
    CliAdapter cli{project_analyzer, impact_analyzer, report_generator};
    std::ostringstream out;
    std::ostringstream err;

    int run(std::initializer_list<const char*> args) {
        std::vector<const char*> argv_vec = {"cmake-xray"};
        argv_vec.insert(argv_vec.end(), args);
        return cli.run(static_cast<int>(argv_vec.size()), argv_vec.data(), out, err);
    }
};

const std::string testdata = "tests/e2e/testdata/";

class StubAnalyzeProjectPort final : public xray::hexagon::ports::driving::AnalyzeProjectPort {
public:
    explicit StubAnalyzeProjectPort(AnalysisResult result) : result_(std::move(result)) {}

    AnalysisResult analyze_project(std::string_view /*compile_commands_path*/) const override {
        return result_;
    }

private:
    AnalysisResult result_;
};

class StubAnalyzeImpactPort final : public xray::hexagon::ports::driving::AnalyzeImpactPort {
public:
    ImpactResult analyze_impact(std::string_view /*compile_commands_path*/,
                                const std::filesystem::path& /*changed_path*/) const override {
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
    CHECK(out.str().find("--top") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "impact --help returns exit 0 with help on stdout") {
    CHECK(run({"impact", "--help"}) == ExitCode::success);
    CHECK(out.str().find("--changed-file") != std::string::npos);
    CHECK(out.str().find("relative paths are interpreted relative") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze success path renders ranking and hotspots") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "m2/analyze/compile_commands.json").c_str(), "--top", "2"}) ==
          ExitCode::success);
    CHECK(out.str().find("translation unit ranking") != std::string::npos);
    CHECK(out.str().find("top 2 of 3 translation units") != std::string::npos);
    CHECK(out.str().find("include hotspots [heuristic]") != std::string::npos);
    CHECK(out.str().find("warning: could not resolve include") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "impact success path for direct source match is not heuristic") {
    CHECK(run({"impact", "--compile-commands",
               (testdata + "m2/analyze/compile_commands.json").c_str(), "--changed-file",
               "src/app/main.cpp"}) == ExitCode::success);
    CHECK(out.str().find("impact analysis for src/app/main.cpp") != std::string::npos);
    CHECK(out.str().find("[heuristic]") == std::string::npos);
    CHECK(out.str().find("affected translation units: 1") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact success path for header match is heuristic") {
    CHECK(run({"impact", "--compile-commands",
               (testdata + "m2/analyze/compile_commands.json").c_str(), "--changed-file",
               "include/common/config.h"}) == ExitCode::success);
    CHECK(out.str().find("impact analysis for include/common/config.h [heuristic]") !=
          std::string::npos);
    CHECK(out.str().find("affected translation units: 3") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact path semantics normalize lexical relative paths") {
    CHECK(run({"impact", "--compile-commands",
               (testdata + "m2/analyze/compile_commands.json").c_str(), "--changed-file",
               "./include/common/../common/config.h"}) == ExitCode::success);
    CHECK(out.str().find("impact analysis for include/common/config.h [heuristic]") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact missing file keeps exit 0 and explains missing data") {
    CHECK(run({"impact", "--compile-commands",
               (testdata + "m2/analyze/compile_commands.json").c_str(), "--changed-file",
               "include/generated/version.h"}) == ExitCode::success);
    CHECK(out.str().find("affected translation units: 0") != std::string::npos);
    CHECK(out.str().find("not present in the loaded compile database") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "missing analyze arguments returns exit 2") {
    CHECK(run({"analyze"}) == ExitCode::cli_usage_error);
}

TEST_CASE_FIXTURE(CliFixture, "missing impact changed-file returns exit 2") {
    CHECK(run({"impact", "--compile-commands",
               (testdata + "m2/analyze/compile_commands.json").c_str()}) ==
          ExitCode::cli_usage_error);
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
    const StubGenerateReportPort generate_report_port;
    const CliAdapter cli{analyze_project_port, analyze_impact_port, generate_report_port};
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
    const StubGenerateReportPort generate_report_port;
    const CliAdapter cli{analyze_project_port, analyze_impact_port, generate_report_port};
    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "analyze", "--compile-commands",
                          "/tmp/compile_commands.json"};

    CHECK(cli.run(4, argv, out, err) == ExitCode::unexpected_error);
    CHECK(err.str().find("unexpected compile database failure") != std::string::npos);
}
