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

#include "cli_test_support.h"

using namespace xray::tests::cli_support;

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

// ---- AP M5-1.6 Tranche A: --version-Flag ---------------------------------
//
// `cmake-xray --version` ist ein globales Flag, das CLI11 vor jedem
// Subcommand-Dispatch behandelt. Die Ausgabe ist die kanonische App-Version
// ohne fuehrendes 'v', Exit 0; die Analyse-Pipeline wird nie gestartet.
// XRAY_APP_VERSION_STRING ist das Build-Time-Define aus der Root-CMakeLists
// und die einzige Quelle der Wahrheit fuer die Assertions.

TEST_CASE_FIXTURE(CliFixture, "cmake-xray --version prints app version on stdout and exits 0") {
    CHECK(run({"--version"}) == ExitCode::success);
    CHECK(out.str() == std::string{XRAY_APP_VERSION_STRING} + "\n");
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "cmake-xray --version output matches application_info().version") {
    // Konsistenz-Test (plan-M5-1-6.md DoD Tranche A): die --version-Ausgabe,
    // application_info().version und XRAY_APP_VERSION_STRING muessen
    // uebereinstimmen.
    CHECK(run({"--version"}) == ExitCode::success);
    const std::string expected =
        std::string{xray::hexagon::model::application_info().version} + "\n";
    CHECK(out.str() == expected);
    CHECK(std::string_view{XRAY_APP_VERSION_STRING} ==
          xray::hexagon::model::application_info().version);
}

TEST_CASE_FIXTURE(CliFixture, "cmake-xray --version output has no leading v") {
    CHECK(run({"--version"}) == ExitCode::success);
    REQUIRE_FALSE(out.str().empty());
    CHECK(out.str().front() != 'v');
}

TEST_CASE("cmake-xray --version short-circuits before subcommand dispatch") {
    // Stub-Ports duerfen nicht aufgerufen werden, wenn --version das einzige
    // Argument ist; CLI11 wirft CLI::CallForVersion vor dem Subcommand-
    // Callback, und das Versions-Flag funktioniert auch ohne
    // --compile-commands oder --cmake-file-api.
    AnalysisResult unused_analysis;
    unused_analysis.application = xray::hexagon::model::application_info();
    unused_analysis.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    const StubAnalyzeProjectPort analyze_port{unused_analysis};
    const StubAnalyzeImpactPort impact_port;
    const StubGenerateReportPort console_port;
    const StubGenerateReportPort markdown_port;
    const StubGenerateReportPort json_port;
    const StubGenerateReportPort dot_port;
    const StubGenerateReportPort html_port;
    const xray::adapters::cli::ReportPorts ports{console_port, markdown_port, json_port,
                                                  dot_port, html_port};
    const CliAdapter cli{analyze_port, impact_port, ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray", "--version"};
    CHECK(cli.run(2, argv, out, err) == ExitCode::success);
    CHECK(out.str() == std::string{XRAY_APP_VERSION_STRING} + "\n");
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze success path renders ranking and hotspots") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");

    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--top", "2"}) ==
          ExitCode::success);
    CHECK(out.str().find("translation unit ranking") != std::string::npos);
    CHECK(out.str().find("top 2 of 3 translation units") != std::string::npos);
    // AP M6-1.4 A.5 step 23: v4 Include Hotspots heading.
    CHECK(out.str().find("Include Hotspots (scope=all, depth=all;") != std::string::npos);
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
    // AP M6-1.4 A.5 step 23: v4 per-hotspot line format with origin/depth
    // suffix replacing the M3 "(affected translation units: N)" wording.
    CHECK(out.str().find("include/common/config.h [project, direct] (3 translation units)") !=
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

// AP M5-1.8 A.5 audit fixup: plan-M5-1-8.md "1.8 Scope" Bullet 22
// (CLI-/E2E-Negativtests fuer File-API-only-Impact-Laeufe mit
// changed_file_source=unresolved_file_api_source_root) verlangt
// `impact --format json` und `impact --format json --output <path>`
// ausdruecklich auf CLI-Ebene, "damit der JSON-v1-Vertrag 'Textfehler ohne
// JSON-Report' nicht nur auf Schema-/Adapterebene abgesichert ist". Die
// Schema-Negativtests in test_json_report_adapter.cpp pinnen das Verhalten
// auf Adapterebene; diese zwei Tests pinnen es jetzt auch auf CLI-Ebene
// (parallel zur DOT- und HTML-Coverage oben).
TEST_CASE("json impact refuses to render when changed_file_source is unresolved_file_api_source_root") {
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
                          "--format",       "json"};

    const int exit_code = cli.run(8, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("file-api source root is unresolved") != std::string::npos);
    // No JSON document should leak into stderr either.
    CHECK(err.str().find("\"format\":") == std::string::npos);
    CHECK(err.str().find("\"format_version\":") == std::string::npos);
}

TEST_CASE("json impact --output refuses to render when changed_file_source is unresolved_file_api_source_root") {
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

    // Use a per-test temp dir so we can assert "no target file was
    // created". The test-stage filesystem is per-container; we wipe any
    // leftover dir from a previous run before asserting the target does
    // not exist.
    const auto target_dir = std::filesystem::temp_directory_path() /
                            "cmake-xray-json-unresolved-output";
    std::error_code wipe_ec;
    std::filesystem::remove_all(target_dir, wipe_ec);
    std::filesystem::create_directories(target_dir);
    const auto target = target_dir / "impact.json";
    REQUIRE_FALSE(std::filesystem::exists(target));

    std::ostringstream out;
    std::ostringstream err;
    const std::string target_arg = target.string();
    const char* argv[] = {"cmake-xray",     "impact",         "--cmake-file-api",
                          "/tmp/empty",     "--changed-file", "src/missing.cpp",
                          "--format",       "json",
                          "--output",       target_arg.c_str()};

    const int exit_code = cli.run(10, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("file-api source root is unresolved") != std::string::npos);
    // The render precondition fires before the writer is invoked, so the
    // target file must not exist after the call.
    CHECK_FALSE(std::filesystem::exists(target));

    std::error_code ec;
    std::filesystem::remove_all(target_dir, ec);
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

// ---- AP M6-1.3 A.2: --impact-target-depth + --require-target-graph ----

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --impact-target-depth abc returns exit 2 with 'not an integer'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--impact-target-depth", "abc"}) ==
          ExitCode::cli_usage_error);
    CHECK(err.str().find("--impact-target-depth: not an integer") != std::string::npos);
    CHECK(out.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --impact-target-depth 1.5 returns exit 2 with 'not an integer'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--impact-target-depth", "1.5"}) ==
          ExitCode::cli_usage_error);
    CHECK(err.str().find("--impact-target-depth: not an integer") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --impact-target-depth -1 returns exit 2 with 'negative value'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--impact-target-depth", "-1"}) ==
          ExitCode::cli_usage_error);
    CHECK(err.str().find("--impact-target-depth: negative value") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --impact-target-depth 33 returns exit 2 with 'value exceeds maximum 32'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--impact-target-depth", "33"}) ==
          ExitCode::cli_usage_error);
    CHECK(err.str().find("--impact-target-depth: value exceeds maximum 32") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --impact-target-depth 100000 (5+ digits) returns exit 2 with 'value exceeds maximum 32'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--impact-target-depth", "999999"}) ==
          ExitCode::cli_usage_error);
    CHECK(err.str().find("--impact-target-depth: value exceeds maximum 32") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --impact-target-depth specified twice returns exit 2 with 'option specified more than once'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--impact-target-depth", "2",
               "--impact-target-depth", "3"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--impact-target-depth: option specified more than once") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --impact-target-depth 0 is accepted") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--impact-target-depth", "0"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --impact-target-depth 32 is accepted at the upper bound") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--impact-target-depth", "32"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --require-target-graph without target graph returns exit 1") {
    // Compile-DB-only run: target_graph_status=not_loaded; --require-target-graph
    // promotes that to a hard CLI failure with the documented message.
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--require-target-graph"}) ==
          ExitCode::unexpected_error);
    CHECK(err.str().find("--require-target-graph: target graph data is required "
                          "but not available") != std::string::npos);
    CHECK(out.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.3 A.2: --require-target-graph + --impact-target-depth 0 keeps the documented combo error mapping") {
    // Even with depth=0 (which makes BFS a no-op), --require-target-graph
    // still demands a usable target graph; without one, exit 1 fires.
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--require-target-graph",
               "--impact-target-depth", "0"}) == ExitCode::unexpected_error);
    CHECK(err.str().find("--require-target-graph: target graph data is required "
                          "but not available") != std::string::npos);
}

// ---- AP M6-1.4 A.3: --include-scope + --include-depth ----

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope unknown-token returns exit 2 with 'invalid value'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-scope",
               "bogus"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--include-scope: invalid value 'bogus'; allowed: all, project, "
                          "external, unknown") != std::string::npos);
    CHECK(out.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope specified twice returns exit 2 with 'option specified more than once'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-scope",
               "all", "--include-scope", "project"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--include-scope: option specified more than once") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope= (empty value) is rejected with cli_usage_error") {
    // CLI11 catches a typed-but-empty value at parse time before our
    // validator runs, so the exit code is asserted but the exact phrase
    // (CLI11's default text) is not part of the AP 1.4 contract.
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(),
               "--include-scope="}) == ExitCode::cli_usage_error);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope all is accepted (explicit default)") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-scope",
               "all"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope project is accepted") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-scope",
               "project"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope=external is accepted") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(),
               "--include-scope=external"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope=unknown is accepted") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(),
               "--include-scope=unknown"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope=' project' (leading whitespace) is invalid value") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(),
               "--include-scope= project"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--include-scope: invalid value ' project'") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope='project ' (trailing whitespace) is invalid value") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(),
               "--include-scope=project "}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--include-scope: invalid value 'project '") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-depth='direct ' (trailing whitespace) is invalid value") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(),
               "--include-depth=direct "}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--include-depth: invalid value 'direct '") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope project,external (comma) is invalid value") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-scope",
               "project,external"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--include-scope: invalid value 'project,external'") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-depth unknown-token returns exit 2 with 'invalid value'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-depth",
               "deep"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--include-depth: invalid value 'deep'; allowed: all, direct, "
                          "indirect") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-depth specified twice returns exit 2 with 'option specified more than once'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-depth",
               "all", "--include-depth", "direct"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--include-depth: option specified more than once") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-depth= (empty value) is rejected with cli_usage_error") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(),
               "--include-depth="}) == ExitCode::cli_usage_error);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-depth all is accepted (explicit default)") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-depth",
               "all"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-depth direct is accepted") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-depth",
               "direct"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-depth=indirect is accepted") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(),
               "--include-depth=indirect"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope and --include-depth compose into the analyze run") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--include-scope",
               "project", "--include-depth", "direct"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.4 A.3: --include-scope and --include-depth are not exposed on impact") {
    // Plan-M6-1-4: only analyze accepts the new filters; impact rejects them
    // with CLI11's standard "unrecognised option" parse error (exit 2).
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--include-scope", "project"}) ==
          ExitCode::cli_usage_error);
}

// ---- AP M6-1.5 A.1: --analysis ----

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --analysis omitted uses the default (all sections)") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str()}) ==
          ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture, "AP1.5 A.1: --analysis all is accepted (explicit default)") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "all"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture, "AP1.5 A.1: --analysis tu-ranking is accepted") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "tu-ranking"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(
    CliFixture,
    "AP1.5 A.1: --analysis target-graph,target-hubs is accepted (dependency satisfied)") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "target-graph,target-hubs"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --analysis trims ASCII whitespace around comma-separated tokens") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               " tu-ranking , include-hotspots "}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --analysis bogus returns exit 2 with 'unknown analysis' phrase") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "bogus"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--analysis: unknown analysis 'bogus'; allowed: all, tu-ranking, "
                          "include-hotspots, target-graph, target-hubs") != std::string::npos);
}

TEST_CASE_FIXTURE(
    CliFixture,
    "AP1.5 A.1: --analysis all combined with another value returns exit 2 with 'all must not be combined'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "all,tu-ranking"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--analysis: 'all' must not be combined with other analysis values") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --analysis with a duplicate value returns exit 2 with 'duplicate' phrase") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "tu-ranking,tu-ranking"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--analysis: duplicate analysis value 'tu-ranking'") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --analysis target-hubs without target-graph is rejected") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "target-hubs"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--analysis: target-hubs requires target-graph") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(
    CliFixture,
    "AP1.5 A.1: --analysis target-hubs,tu-ranking without target-graph is rejected") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "target-hubs,tu-ranking"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--analysis: target-hubs requires target-graph") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --analysis with an empty token returns exit 2 with 'empty analysis value' phrase") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "tu-ranking,,target-graph"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--analysis: empty analysis value in list") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --analysis specified twice returns exit 2 with 'option specified more than once'") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--analysis",
               "tu-ranking", "--analysis", "target-graph"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--analysis: option specified more than once") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "AP1.5 A.1: --analysis is not exposed on impact") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--analysis", "tu-ranking"}) == ExitCode::cli_usage_error);
}

// ---- AP M6-1.5 A.1: --tu-threshold ----

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --tu-threshold arg_count=5 is accepted") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--tu-threshold",
               "arg_count=5"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --tu-threshold accepts multiple metrics, one per option") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--tu-threshold",
               "arg_count=5", "--tu-threshold", "include_path_count=2", "--tu-threshold",
               "define_count=1"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(CliFixture, "AP1.5 A.1: --tu-threshold arg_count=0 is accepted") {
    // Plan §359-361: a threshold of 0 is semantically "no filter" and
    // must still be accepted so scripts can pass through values
    // unconditionally.
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--tu-threshold",
               "arg_count=0"}) == ExitCode::success);
}

TEST_CASE_FIXTURE(
    CliFixture,
    "AP1.5 A.1: --tu-threshold without '=' returns exit 2 with 'invalid syntax' phrase") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--tu-threshold",
               "arg_count5"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--tu-threshold: invalid syntax 'arg_count5'; expected <metric>=<n>") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(
    CliFixture,
    "AP1.5 A.1: --tu-threshold with unknown metric returns exit 2 with 'unknown metric' phrase") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--tu-threshold",
               "bogus=5"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--tu-threshold: unknown metric 'bogus'; allowed: arg_count, "
                          "include_path_count, define_count") != std::string::npos);
}

TEST_CASE_FIXTURE(
    CliFixture,
    "AP1.5 A.1: --tu-threshold with non-numeric value returns exit 2 with 'not an integer' phrase") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--tu-threshold",
               "arg_count=abc"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--tu-threshold: not an integer") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                   "AP1.5 A.1: --tu-threshold with negative value is rejected") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--tu-threshold",
               "arg_count=-1"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--tu-threshold: negative value") != std::string::npos);
}

TEST_CASE_FIXTURE(
    CliFixture,
    "AP1.5 A.1: --tu-threshold with a duplicate metric returns exit 2 with 'duplicate metric' phrase") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"analyze", "--compile-commands", compile_commands.c_str(), "--tu-threshold",
               "arg_count=5", "--tu-threshold", "arg_count=7"}) == ExitCode::cli_usage_error);
    CHECK(err.str().find("--tu-threshold: duplicate metric 'arg_count'") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "AP1.5 A.1: --tu-threshold is not exposed on impact") {
    const auto compile_commands = fixture_path("m2/basic_project/compile_commands.json");
    CHECK(run({"impact", "--compile-commands", compile_commands.c_str(), "--changed-file",
               "src/app/main.cpp", "--tu-threshold", "arg_count=5"}) ==
          ExitCode::cli_usage_error);
}
