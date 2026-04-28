#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "cli_test_support.h"

using namespace xray::tests::cli_support;

// ---- AP M5-1.5 Tranche A: --quiet / --verbose parser contract ----------
//
// The parser-contract tests below pin the command-local registration, the
// mutual-exclusion rule and the precedence interactions with --output,
// --changed-file and --format console. Tranche A delivers no functional
// emission change yet: --quiet and --verbose are accepted, mutually
// exclusive, and reportinhaltlich a noop for every artifact format. Console
// quiet/verbose renderers and verbose stderr context land in Tranche B/C.

TEST_CASE_FIXTURE(CliFixture, "analyze --quiet is accepted as a command-local flag") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
                 "--top", "2"}) == ExitCode::success);
    CHECK_FALSE(out.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze --verbose is accepted as a command-local flag") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
                 "--top", "2"}) == ExitCode::success);
    CHECK_FALSE(out.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "impact --quiet is accepted as a command-local flag") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--quiet"}) ==
            ExitCode::success);
    CHECK_FALSE(out.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "impact --verbose is accepted as a command-local flag") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--verbose"}) ==
            ExitCode::success);
    CHECK_FALSE(out.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet --verbose is rejected as mutual-exclusion error") {
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
               "--verbose"}) == ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("--quiet and --verbose are mutually exclusive") != std::string::npos);
    CHECK(err.str().find("hint:") != std::string::npos);
    // AP M5-1.5 Tranche C.1: Mutual-Exclusion ist Praezedenzstufe 1 und greift
    // vor jedem Verbose-Kontext-Helfer; stderr darf hier kein verbose:-Praefix
    // enthalten.
    CHECK(err.str().find("verbose:") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --quiet --verbose is rejected as mutual-exclusion error") {
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(),
               "--changed-file", "include/common/shared.h", "--quiet", "--verbose"}) ==
          ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("--quiet and --verbose are mutually exclusive") != std::string::npos);
    CHECK(err.str().find("verbose:") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "global --quiet position before subcommand is rejected with nonzero exit") {
    // Plan: AP 1.5 documents only nonzero exit and non-empty stderr for the
    // global position; the exact CLI11 wording is not part of the contract.
    CHECK(run({"--quiet", "analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str()}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "global --verbose position before subcommand is rejected with nonzero exit") {
    CHECK(run({"--verbose", "impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(),
               "--changed-file", "include/common/shared.h"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose without --changed-file returns Usage-Fehler ohne verbose-Block") {
    // Pflichtoptionen-Pruefung (Praezedenzstufe 4) greift vor jedem
    // Verbose-Kontext aus Tranche C. stderr enthaelt nur die normale Usage-
    // Fehlermeldung; kein verbose:-Praefix darf hier auftauchen.
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose"}) ==
          ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("impact requires --changed-file") != std::string::npos);
    CHECK(err.str().find("verbose:") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet --format console --output rejects with the documented two-line error") {
    const TemporaryDirectory temp_dir;
    const auto target = (temp_dir.path() / "report.txt").string();
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
               "--format", "console", "--output", target.c_str()}) ==
          ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("error: --output is not supported with --format console") !=
          std::string::npos);
    CHECK(err.str().find("hint: use an artifact-oriented format") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(target));
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose --format console --output rejects with the documented two-line error") {
    const TemporaryDirectory temp_dir;
    const auto target = (temp_dir.path() / "report.txt").string();
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(),
               "--changed-file", "include/common/shared.h", "--verbose", "--format", "console",
               "--output", target.c_str()}) == ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("error: --output is not supported with --format console") !=
          std::string::npos);
    CHECK_FALSE(std::filesystem::exists(target));
}

// ---- AP M5-1.5 Tranche A: artifact noop policy --------------------------
//
// Quiet and Verbose must not change report content for any artifact format.
// stdout (without --output) is byte-identical to the normal-mode run; the
// file written via --output is byte-identical and stdout/stderr stay empty
// in Tranche A. Verbose stderr context lights up only in Tranche C.

namespace {

std::string run_normal_mode_stdout(CliFixture& fixture,
                                    std::initializer_list<const char*> args) {
    fixture.run(args);
    return fixture.out.str();
}

}  // namespace

TEST_CASE_FIXTURE(CliFixture, "analyze --quiet --format markdown stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"analyze", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                "markdown", "--top", "2"});
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
                 "--format", "markdown", "--top", "2"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze --verbose --format markdown stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"analyze", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                "markdown", "--top", "2"});
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "markdown", "--top", "2"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    // AP M5-1.5 Tranche B.2: verbose --format <artifact> emits the documented
    // verbose: stderr block. Detailed line-by-line assertions live in the
    // Tranche B.2 verbose-stderr-block test cases below; here we just confirm
    // the block is wired up.
    CHECK(err.str().find("verbose: report_type=analyze\n") != std::string::npos);
    CHECK(err.str().find("verbose: format=markdown\n") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "analyze --quiet --format json stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"analyze", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                "json", "--top", "2"});
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
                 "--format", "json", "--top", "2"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze --verbose --format json stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"analyze", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                "json", "--top", "2"});
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "json", "--top", "2"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().find("verbose: report_type=analyze\n") != std::string::npos);
    CHECK(err.str().find("verbose: format=json\n") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "analyze --quiet --format dot stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"analyze", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                "dot", "--top", "2"});
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
                 "--format", "dot", "--top", "2"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze --verbose --format dot stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"analyze", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                "dot", "--top", "2"});
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "dot", "--top", "2"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().find("verbose: report_type=analyze\n") != std::string::npos);
    CHECK(err.str().find("verbose: format=dot\n") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "analyze --quiet --format html stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"analyze", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                "html", "--top", "2"});
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
                 "--format", "html", "--top", "2"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze --verbose --format html stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"analyze", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--format",
                "html", "--top", "2"});
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "html", "--top", "2"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().find("verbose: report_type=analyze\n") != std::string::npos);
    CHECK(err.str().find("verbose: format=html\n") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact --quiet --format markdown stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"impact", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--changed-file",
                "include/common/shared.h", "--format", "markdown"});
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--quiet", "--format",
                 "markdown"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "impact --verbose --format json stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"impact", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--changed-file",
                "include/common/shared.h", "--format", "json"});
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--verbose", "--format",
                 "json"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().find("verbose: report_type=impact\n") != std::string::npos);
    CHECK(err.str().find("verbose: format=json\n") != std::string::npos);
    CHECK(err.str().find("verbose: changed_file_source=") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "impact --quiet --format dot stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"impact", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--changed-file",
                "include/common/shared.h", "--format", "dot"});
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--quiet", "--format",
                 "dot"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "impact --verbose --format html stdout matches normal mode") {
    const auto baseline = run_normal_mode_stdout(
        *this, {"impact", "--compile-commands",
                "tests/e2e/testdata/m2/basic_project/compile_commands.json", "--changed-file",
                "include/common/shared.h", "--format", "html"});
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--verbose", "--format",
                 "html"}) == ExitCode::success);
    CHECK(out.str() == baseline);
    CHECK(err.str().find("verbose: report_type=impact\n") != std::string::npos);
    CHECK(err.str().find("verbose: format=html\n") != std::string::npos);
    CHECK(err.str().find("verbose: changed_file_source=") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet --format json --output writes byte-identical file with empty streams") {
    const TemporaryDirectory temp_dir;
    const auto baseline_target = (temp_dir.path() / "baseline.json").string();
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
                 "json", "--output", baseline_target.c_str(), "--top", "2"}) ==
            ExitCode::success);
    const auto baseline = read_text_file(std::filesystem::path{baseline_target});

    const auto quiet_target = (temp_dir.path() / "quiet.json").string();
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
                 "--format", "json", "--output", quiet_target.c_str(), "--top", "2"}) ==
            ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    CHECK(read_text_file(std::filesystem::path{quiet_target}) == baseline);
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose --format json --output writes byte-identical file with empty streams in Tranche A") {
    const TemporaryDirectory temp_dir;
    const auto baseline_target = (temp_dir.path() / "baseline.json").string();
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
                 "json", "--output", baseline_target.c_str(), "--top", "2"}) ==
            ExitCode::success);
    const auto baseline = read_text_file(std::filesystem::path{baseline_target});

    const auto verbose_target = (temp_dir.path() / "verbose.json").string();
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "json", "--output", verbose_target.c_str(), "--top", "2"}) ==
            ExitCode::success);
    CHECK(out.str().empty());
    // AP M5-1.5 Tranche B.2: verbose --format <artifact> --output emits the
    // documented verbose: stderr block; output=file confirms the --output
    // dispatch took effect.
    CHECK(err.str().find("verbose: format=json\n") != std::string::npos);
    CHECK(err.str().find("verbose: output=file\n") != std::string::npos);
    CHECK(read_text_file(std::filesystem::path{verbose_target}) == baseline);
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --quiet --format html --output writes byte-identical file with empty streams") {
    const TemporaryDirectory temp_dir;
    const auto baseline_target = (temp_dir.path() / "baseline.html").string();
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--format", "html", "--output",
                 baseline_target.c_str()}) == ExitCode::success);
    const auto baseline = read_text_file(std::filesystem::path{baseline_target});

    const auto quiet_target = (temp_dir.path() / "quiet.html").string();
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--quiet", "--format", "html",
                 "--output", quiet_target.c_str()}) == ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().empty());
    CHECK(read_text_file(std::filesystem::path{quiet_target}) == baseline);
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose --format dot --output writes byte-identical file with empty streams in Tranche A") {
    const TemporaryDirectory temp_dir;
    const auto baseline_target = (temp_dir.path() / "baseline.dot").string();
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--format", "dot", "--output",
                 baseline_target.c_str()}) == ExitCode::success);
    const auto baseline = read_text_file(std::filesystem::path{baseline_target});

    const auto verbose_target = (temp_dir.path() / "verbose.dot").string();
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--verbose", "--format", "dot",
                 "--output", verbose_target.c_str()}) == ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("verbose: report_type=impact\n") != std::string::npos);
    CHECK(err.str().find("verbose: format=dot\n") != std::string::npos);
    CHECK(err.str().find("verbose: output=file\n") != std::string::npos);
    CHECK(err.str().find("verbose: changed_file_source=") != std::string::npos);
    CHECK(read_text_file(std::filesystem::path{verbose_target}) == baseline);
}

// ---- AP M5-1.5 Tranche B.1: Console Quiet end-to-end --------------------
//
// The Quiet renderer's textual contract is locked down by adapter unit tests
// in tests/adapters/test_cli_console_renderers.cpp; the CLI-level tests below
// verify that --format console --quiet wires through the cli_adapter dispatch
// and matches the byte-stable goldens under
// tests/e2e/testdata/m5/verbosity/. e2e_binary smokes for the same goldens
// land in Tranche C.2.

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet --format console matches console-analyze-quiet golden") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m3/report_project/compile_commands.json").c_str(), "--quiet",
                 "--format", "console", "--top", "2"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-analyze-quiet.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet console with targets matches console-analyze-quiet-targets golden") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m4/with_targets/compile_commands.json").c_str(),
                 "--cmake-file-api", fixture_path("m4/with_targets/build").c_str(),
                 "--quiet", "--format", "console", "--top", "2"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-analyze-quiet-targets.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet console for the lean fixture matches console-analyze-quiet-empty golden") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m5/dot-fixtures/multi_tu_compile_db/compile_commands.json")
                     .c_str(),
                 "--quiet", "--format", "console"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-analyze-quiet-empty.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --quiet --format console matches console-impact-quiet golden") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m3/report_impact_header/compile_commands.json").c_str(),
                 "--changed-file", "include/common/config.h", "--quiet", "--format",
                 "console"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-impact-quiet.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --quiet console with targets matches console-impact-quiet-targets golden") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m4/with_targets/compile_commands.json").c_str(),
                 "--cmake-file-api", fixture_path("m4/with_targets/build").c_str(),
                 "--changed-file", "include/common/shared.h", "--quiet", "--format",
                 "console"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-impact-quiet-targets.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --quiet console with no affected TUs matches console-impact-quiet-empty golden") {
    REQUIRE(run({"impact", "--cmake-file-api",
                 fixture_path("m4/file_api_only/build").c_str(), "--changed-file",
                 "include/common/shared.h", "--quiet", "--format", "console"}) ==
            ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-impact-quiet-empty.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

// AP M5-1.5 Tranche B.1: render preconditions for Console Quiet impact mirror
// the HTML pathway from AP 1.4. A stub ImpactPort returns a result with
// inputs.changed_file == std::nullopt or inputs.changed_file_source ==
// unresolved_file_api_source_root; the CLI must reject before the Quiet
// renderer runs so no partial console output appears on stdout.

TEST_CASE("console --quiet impact rejects when changed_file_source is unresolved_file_api_source_root") {
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
    const char* argv[] = {"cmake-xray", "impact",         "--cmake-file-api",
                          "/tmp/empty", "--changed-file", "src/missing.cpp",
                          "--quiet"};

    const int exit_code = cli.run(7, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("file-api source root is unresolved") != std::string::npos);
    CHECK(err.str().find("--format console --quiet") != std::string::npos);
    // No partial console-quiet output must reach stderr either.
    CHECK(err.str().find("impact: ok") == std::string::npos);
}

TEST_CASE("console --verbose impact rejects when changed_file_source is unresolved_file_api_source_root") {
    // The precondition predicate covers Quiet AND Verbose console; Tranche B.1
    // wires the test for Verbose now so the precondition branch stays covered
    // even though the Verbose renderer itself only lands in B.2.
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
    const char* argv[] = {"cmake-xray", "impact",         "--cmake-file-api",
                          "/tmp/empty", "--changed-file", "src/missing.cpp",
                          "--verbose"};

    const int exit_code = cli.run(7, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("file-api source root is unresolved") != std::string::npos);
    CHECK(err.str().find("--format console --verbose") != std::string::npos);
}

TEST_CASE("console --quiet impact rejects when changed_file is std::nullopt") {
    ImpactResult unset_result{};
    unset_result.application = xray::hexagon::model::application_info();
    unset_result.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    // inputs.changed_file stays std::nullopt deliberately.

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
                          "--quiet"};

    const int exit_code = cli.run(7, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("changed_file is missing") != std::string::npos);
    CHECK(err.str().find("--format console --quiet") != std::string::npos);
    CHECK(err.str().find("impact: ok") == std::string::npos);
}

// ---- AP M5-1.5 Tranche B.2: Console Verbose end-to-end ------------------
//
// CLI-level smokes that route --format console --verbose through the
// dispatch in cli_adapter.cpp and match each documented case against its
// byte-stable golden under tests/e2e/testdata/m5/verbosity/. e2e_binary
// smokes for the same goldens land in Tranche C.2.

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose --format console matches console-analyze-verbose golden") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m3/report_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "console", "--top", "2"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-analyze-verbose.txt"));
    CHECK(out.str() == golden);
    // Tranche B.2 keeps console verbose stderr empty for the success path;
    // verbose stderr is documented for artifact formats only.
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose console with targets matches console-analyze-verbose-targets golden") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m4/with_targets/compile_commands.json").c_str(),
                 "--cmake-file-api", fixture_path("m4/with_targets/build").c_str(),
                 "--verbose", "--format", "console", "--top", "2"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-analyze-verbose-targets.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose console for file-api-only matches console-analyze-verbose-file-api-only golden") {
    REQUIRE(run({"analyze", "--cmake-file-api",
                 fixture_path("m4/file_api_only/build").c_str(), "--verbose", "--format",
                 "console", "--top", "5"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-analyze-verbose-file-api-only.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose console for the lean fixture matches console-analyze-verbose-empty golden") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m5/dot-fixtures/multi_tu_compile_db/compile_commands.json")
                     .c_str(),
                 "--verbose", "--format", "console"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-analyze-verbose-empty.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose --format console matches console-impact-verbose golden") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m3/report_impact_header/compile_commands.json").c_str(),
                 "--changed-file", "include/common/config.h", "--verbose", "--format",
                 "console"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-impact-verbose.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose console for direct match matches console-impact-verbose-direct golden") {
    REQUIRE(run({"impact", "--cmake-file-api",
                 fixture_path("m4/file_api_only/build").c_str(), "--changed-file",
                 "src/main.cpp", "--verbose", "--format", "console"}) ==
            ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-impact-verbose-direct.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose console with targets matches console-impact-verbose-targets golden") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m4/with_targets/compile_commands.json").c_str(),
                 "--cmake-file-api", fixture_path("m4/with_targets/build").c_str(),
                 "--changed-file", "include/common/shared.h", "--verbose", "--format",
                 "console"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-impact-verbose-targets.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose console with no affected TUs matches console-impact-verbose-empty golden") {
    REQUIRE(run({"impact", "--cmake-file-api",
                 fixture_path("m4/file_api_only/build").c_str(), "--changed-file",
                 "include/common/shared.h", "--verbose", "--format", "console"}) ==
            ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-impact-verbose-empty.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

// ---- AP M5-1.5 Tranche B.2: Verbose-Artefakt-stderr exact-line tests ----
//
// The verbose stderr block is contractually 7 lines for analyze and 8 lines
// for impact, in the documented order, with no top_limit entry. These tests
// pin the exact byte-sequence; the broader Tranche A artifact-noop tests
// already verify substring presence per format/subcommand.

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose --format json emits exactly the documented seven-line stderr block") {
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "json", "--top", "2"}) == ExitCode::success);
    const std::string expected =
        "verbose: report_type=analyze\n"
        "verbose: format=json\n"
        "verbose: output=stdout\n"
        "verbose: compile_database_source=cli\n"
        "verbose: cmake_file_api_source=not_provided\n"
        "verbose: observation_source=exact\n"
        "verbose: target_metadata=not_loaded\n";
    CHECK(err.str() == expected);
    CHECK(err.str().find("top_limit") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose --format json emits exactly the documented eight-line stderr block") {
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", "include/common/shared.h", "--verbose", "--format",
                 "json"}) == ExitCode::success);
    const std::string expected =
        "verbose: report_type=impact\n"
        "verbose: format=json\n"
        "verbose: output=stdout\n"
        "verbose: compile_database_source=cli\n"
        "verbose: cmake_file_api_source=not_provided\n"
        "verbose: observation_source=exact\n"
        "verbose: target_metadata=not_loaded\n"
        "verbose: changed_file_source=compile_database_directory\n";
    CHECK(err.str() == expected);
    CHECK(err.str().find("top_limit") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose --format html --output emits output=file in the verbose block") {
    const TemporaryDirectory temp_dir;
    const auto target = (temp_dir.path() / "report.html").string();
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "html", "--output", target.c_str(), "--top", "2"}) ==
            ExitCode::success);
    CHECK(err.str().find("verbose: output=file\n") != std::string::npos);
    CHECK(err.str().find("verbose: format=html\n") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "console --verbose with successful render keeps stderr empty") {
    // Console verbose stays a stdout-only emitter; the verbose: stderr block
    // is documented for artifact formats only.
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "console", "--top", "2"}) == ExitCode::success);
    CHECK(err.str().empty());
}

// AP M5-1.5 Tranche B.2: cover every ChangedFileSource label branch in the
// verbose-stderr emitter plus the nullopt fallback. The first three reach
// real fixtures; cli_absolute also exercises the file API source root case
// because m4/file_api_only resolves /project/... back to its source root.

TEST_CASE_FIXTURE(CliFixture,
                  "verbose impact stderr labels file_api_source_root for relative changed files via file API") {
    REQUIRE(run({"impact", "--cmake-file-api",
                 fixture_path("m4/file_api_only/build").c_str(), "--changed-file",
                 "src/main.cpp", "--verbose", "--format", "json"}) ==
            ExitCode::success);
    CHECK(err.str().find("verbose: changed_file_source=file_api_source_root\n") !=
          std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "verbose impact stderr labels cli_absolute when --changed-file is absolute") {
    // POSIX accepts /project/... as absolute; Windows requires a drive
    // letter. Same per-platform split as impact_absolute(_windows) goldens
    // in tests/e2e/run_e2e.sh.
#ifdef _WIN32
    const char* absolute_changed_file = "C:/project/src/app/main.cpp";
#else
    const char* absolute_changed_file = "/project/src/app/main.cpp";
#endif
    REQUIRE(run({"impact", "--compile-commands",
                 fixture_path("m2/basic_project/compile_commands.json").c_str(),
                 "--changed-file", absolute_changed_file, "--verbose", "--format",
                 "json"}) == ExitCode::success);
    CHECK(err.str().find("verbose: changed_file_source=cli_absolute\n") !=
          std::string::npos);
}

// Synthesised stub paths: drive the changed_file_source label branches that
// real fixtures cannot reproduce (unresolved_file_api_source_root for json
// and the nullopt fallback). For unresolved we use a non-html format so the
// render-precondition predicate does not reject the result before stderr
// emission.

TEST_CASE("verbose impact stderr labels unresolved_file_api_source_root for non-html artifact paths") {
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

    AnalysisResult unused_analysis;
    unused_analysis.application = xray::hexagon::model::application_info();
    unused_analysis.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    const StubAnalyzeProjectPort analyze_project_port{unused_analysis};
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
                          "--verbose",      "--format",       "json"};

    const int exit_code = cli.run(9, argv, out, err);
    CHECK(exit_code == ExitCode::success);
    CHECK(err.str().find("verbose: changed_file_source=unresolved_file_api_source_root\n") !=
          std::string::npos);
}

TEST_CASE("verbose impact stderr falls back to not_provided when changed_file_source is std::nullopt") {
    ImpactResult result{};
    result.application = xray::hexagon::model::application_info();
    result.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    // changed_file IS set so the missing-changed-file precondition does not
    // fire for json; changed_file_source remains std::nullopt to exercise
    // the fallback branch in emit_artifact_verbose_stderr_impact.
    result.inputs.changed_file = std::string{"src/whatever.cpp"};
    // result.inputs.changed_file_source intentionally left as std::nullopt.

    class FixedImpactPort final : public xray::hexagon::ports::driving::AnalyzeImpactPort {
    public:
        explicit FixedImpactPort(ImpactResult value) : result_(std::move(value)) {}
        ImpactResult analyze_impact(
            xray::hexagon::ports::driving::AnalyzeImpactRequest /*request*/) const override {
            return result_;
        }

    private:
        ImpactResult result_;
    };

    AnalysisResult unused_analysis;
    unused_analysis.application = xray::hexagon::model::application_info();
    unused_analysis.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    const StubAnalyzeProjectPort analyze_project_port{unused_analysis};
    const FixedImpactPort impact_port{result};
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
                          "/tmp/empty",     "--changed-file", "src/whatever.cpp",
                          "--verbose",      "--format",       "json"};

    const int exit_code = cli.run(9, argv, out, err);
    CHECK(exit_code == ExitCode::success);
    CHECK(err.str().find("verbose: changed_file_source=not_provided\n") !=
          std::string::npos);
}

// ---- AP M5-1.5 Tranche C.1: Verbose-Fehlerkontext und Quiet-Fehlerregel ----
//
// Tranche C.1 aktiviert die in Tranche A/B vorbereiteten verbose-stderr-Pfade
// fuer Render-, Schreib-, Eingabe- und Analysefehler. Verbose ergaenzt die
// normale Fehlermeldung um vier verbose:-Zeilen mit dem aus der CLI-Schicht
// gesetzten validation_stage. Quiet ist Fehler-Nicht-Unterdrueckend und enthaelt
// keine verbose:-Zeilen.


// ---- Render errors with --verbose: 5-line stderr sequence ---------------

TEST_CASE("analyze --verbose --format json with throwing renderer emits exact 5-line stderr") {
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
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

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",      "analyze",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--verbose",       "--format", "json"};
    const int exit_code = cli.run(7, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected =
        "error: cannot render report: analysis report exception\n"
        "verbose: command=analyze\n"
        "verbose: format=json\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=render\n";
    CHECK(err.str() == expected);
}

TEST_CASE("analyze --verbose --format markdown with throwing renderer appends verbose render block") {
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
    const StubAnalyzeImpactPort analyze_impact_port;
    const StubGenerateReportPort console_report_port;
    const ThrowingGenerateReportPort markdown_report_port;
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
    const char* argv[] = {"cmake-xray",      "analyze",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--verbose",       "--format", "markdown"};
    const int exit_code = cli.run(7, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("error: cannot render report:") != std::string::npos);
    const std::string expected_suffix =
        "verbose: command=analyze\n"
        "verbose: format=markdown\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=render\n";
    CHECK(err.str().find(expected_suffix) != std::string::npos);
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE("analyze --verbose --format dot with throwing renderer appends verbose render block") {
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
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

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",      "analyze",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--verbose",       "--format", "dot"};
    const int exit_code = cli.run(7, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=analyze\n"
        "verbose: format=dot\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=render\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE("analyze --verbose --format html with throwing renderer appends verbose render block") {
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
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

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",      "analyze",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--verbose",       "--format", "html"};
    const int exit_code = cli.run(7, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=analyze\n"
        "verbose: format=html\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=render\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE("impact --verbose --format json with throwing renderer emits exact 5-line stderr") {
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
    const FixedImpactPort impact_port{make_impact_success_result()};
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
    const CliAdapter cli{analyze_project_port, impact_port, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",      "impact",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--changed-file",  "src/app/main.cpp",
                          "--verbose",       "--format", "json"};
    const int exit_code = cli.run(9, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected =
        "error: cannot render report: impact report exception\n"
        "verbose: command=impact\n"
        "verbose: format=json\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=render\n";
    CHECK(err.str() == expected);
}

TEST_CASE("impact --verbose --format markdown with throwing renderer appends verbose render block") {
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
    const FixedImpactPort impact_port{make_impact_success_result()};
    const StubGenerateReportPort console_report_port;
    const ThrowingGenerateReportPort markdown_report_port;
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
    const char* argv[] = {"cmake-xray",      "impact",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--changed-file",  "src/app/main.cpp",
                          "--verbose",       "--format", "markdown"};
    const int exit_code = cli.run(9, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=impact\n"
        "verbose: format=markdown\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=render\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE("impact --verbose --format dot with throwing renderer appends verbose render block") {
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
    const FixedImpactPort impact_port{make_impact_success_result()};
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
    const CliAdapter cli{analyze_project_port, impact_port, report_ports};

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",      "impact",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--changed-file",  "src/app/main.cpp",
                          "--verbose",       "--format", "dot"};
    const int exit_code = cli.run(9, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=impact\n"
        "verbose: format=dot\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=render\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE("impact --verbose --format html with throwing renderer appends verbose render block") {
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
    const FixedImpactPort impact_port{make_impact_success_result()};
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

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",      "impact",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--changed-file",  "src/app/main.cpp",
                          "--verbose",       "--format", "html"};
    const int exit_code = cli.run(9, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=impact\n"
        "verbose: format=html\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=render\n";
    CHECK(err.str().ends_with(expected_suffix));
}

// ---- Write errors with --verbose: 6-line stderr sequence ----------------

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose --format json --output <missing-dir> emits exact 6-line stderr") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "report.json").string();
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
               "--format", "json", "--output", missing_target.c_str()}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("error: cannot write report:") != std::string::npos);
    CHECK(err.str().find("hint: check the output path") != std::string::npos);
    const std::string expected_suffix =
        "verbose: command=analyze\n"
        "verbose: format=json\n"
        "verbose: output=file\n"
        "verbose: validation_stage=write\n";
    CHECK(err.str().ends_with(expected_suffix));
    CHECK_FALSE(std::filesystem::exists(missing_target));
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose --format markdown --output <missing-dir> appends verbose write block") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "report.md").string();
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
               "--format", "markdown", "--output", missing_target.c_str()}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=analyze\n"
        "verbose: format=markdown\n"
        "verbose: output=file\n"
        "verbose: validation_stage=write\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose --format dot --output <missing-dir> appends verbose write block") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "report.dot").string();
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
               "--format", "dot", "--output", missing_target.c_str()}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=analyze\n"
        "verbose: format=dot\n"
        "verbose: output=file\n"
        "verbose: validation_stage=write\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose --format html --output <missing-dir> appends verbose write block") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "report.html").string();
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
               "--format", "html", "--output", missing_target.c_str()}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=analyze\n"
        "verbose: format=html\n"
        "verbose: output=file\n"
        "verbose: validation_stage=write\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose --format json --output <missing-dir> emits exact 6-line stderr") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "impact.json").string();
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(),
               "--changed-file", "include/common/shared.h", "--verbose", "--format", "json",
               "--output", missing_target.c_str()}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("error: cannot write report:") != std::string::npos);
    CHECK(err.str().find("hint: check the output path") != std::string::npos);
    const std::string expected_suffix =
        "verbose: command=impact\n"
        "verbose: format=json\n"
        "verbose: output=file\n"
        "verbose: validation_stage=write\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose --format markdown --output <missing-dir> appends verbose write block") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "impact.md").string();
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(),
               "--changed-file", "include/common/shared.h", "--verbose", "--format",
               "markdown", "--output", missing_target.c_str()}) != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=impact\n"
        "verbose: format=markdown\n"
        "verbose: output=file\n"
        "verbose: validation_stage=write\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose --format dot --output <missing-dir> appends verbose write block") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "impact.dot").string();
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(),
               "--changed-file", "include/common/shared.h", "--verbose", "--format", "dot",
               "--output", missing_target.c_str()}) != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=impact\n"
        "verbose: format=dot\n"
        "verbose: output=file\n"
        "verbose: validation_stage=write\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose --format html --output <missing-dir> appends verbose write block") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "impact.html").string();
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(),
               "--changed-file", "include/common/shared.h", "--verbose", "--format", "html",
               "--output", missing_target.c_str()}) != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=impact\n"
        "verbose: format=html\n"
        "verbose: output=file\n"
        "verbose: validation_stage=write\n";
    CHECK(err.str().ends_with(expected_suffix));
}

// ---- Input and analysis errors with --verbose ---------------------------

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose with non-existent compile-database appends verbose input block") {
    CHECK(run({"analyze", "--compile-commands", "/nonexistent/compile_commands.json",
               "--verbose"}) != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=analyze\n"
        "verbose: format=console\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=input\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose with non-existent compile-database appends verbose input block") {
    CHECK(run({"impact", "--compile-commands", "/nonexistent/compile_commands.json",
               "--changed-file", "src/app/main.cpp", "--verbose"}) != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=impact\n"
        "verbose: format=console\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=input\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --verbose with invalid file-api reply appends verbose analysis block") {
    CHECK(run({"analyze", "--cmake-file-api",
               fixture_path("m4/invalid_file_api/build").c_str(), "--verbose"}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=analyze\n"
        "verbose: format=console\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=analysis\n";
    CHECK(err.str().ends_with(expected_suffix));
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose with invalid file-api reply appends verbose analysis block") {
    CHECK(run({"impact", "--cmake-file-api",
               fixture_path("m4/invalid_file_api/build").c_str(), "--changed-file",
               "src/app/main.cpp", "--verbose"}) != ExitCode::success);
    CHECK(out.str().empty());
    const std::string expected_suffix =
        "verbose: command=impact\n"
        "verbose: format=console\n"
        "verbose: output=stdout\n"
        "verbose: validation_stage=analysis\n";
    CHECK(err.str().ends_with(expected_suffix));
}

// ---- Quiet does not suppress errors and emits no verbose: lines ---------

TEST_CASE("analyze --quiet --format json with throwing renderer keeps stderr free of verbose: lines") {
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
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

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",      "analyze",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--quiet",         "--format", "json"};
    const int exit_code = cli.run(7, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("error: cannot render report:") != std::string::npos);
    CHECK(err.str().find("verbose:") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet --format json --output <missing-dir> keeps stderr free of verbose: lines") {
    const TemporaryDirectory temp_dir;
    const auto missing_target = (temp_dir.path() / "missing-dir" / "report.json").string();
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
               "--format", "json", "--output", missing_target.c_str()}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    CHECK(err.str().find("error: cannot write report:") != std::string::npos);
    CHECK(err.str().find("verbose:") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet with non-existent compile-database keeps stderr free of verbose: lines") {
    CHECK(run({"analyze", "--compile-commands", "/nonexistent/compile_commands.json",
               "--quiet"}) != ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("verbose:") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet with invalid file-api reply keeps stderr free of verbose: lines") {
    // Plan code-review follow-up: covers the analysis-stage path (mapped to
    // CompileDatabaseError::file_api_invalid -> stage=analysis). Quiet must
    // surface the standard error message but no verbose: prefix; the four
    // verbose: lines from the C.1 helper only land when --verbose is set.
    CHECK(run({"analyze", "--cmake-file-api",
               fixture_path("m4/invalid_file_api/build").c_str(), "--quiet"}) !=
          ExitCode::success);
    CHECK(out.str().empty());
    CHECK_FALSE(err.str().empty());
    CHECK(err.str().find("verbose:") == std::string::npos);
}

TEST_CASE("analyze normal-mode render error keeps stderr free of verbose: lines") {
    // Negative test: without --verbose the existing render-error stderr stays
    // untouched; no verbose:-Praefix may leak in.
    const StubAnalyzeProjectPort analyze_project_port{make_analyze_success_result()};
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

    std::ostringstream out;
    std::ostringstream err;
    const char* argv[] = {"cmake-xray",      "analyze",
                          "--compile-commands",
                          "tests/e2e/testdata/m2/basic_project/compile_commands.json",
                          "--format",        "json"};
    const int exit_code = cli.run(6, argv, out, err);

    CHECK(exit_code != ExitCode::success);
    CHECK(err.str().find("error: cannot render report:") != std::string::npos);
    CHECK(err.str().find("verbose:") == std::string::npos);
}

// ---- AP M5-1.5 Tranche D: Praezedenz-Cross-Tests ----------------------
//
// Plan-Sektion "Fehlerpraezedenz" listet 9 Stufen. Tranche A/C-Tests pruefen
// jede Stufe einzeln; die Cross-Tests unten pinnen die Reihenfolge, wenn
// gleichzeitig MEHRERE Stufen verletzt sind. Die hoehere Stufe muss gewinnen,
// und Verbose-Block darf erst nach Stufe 4 (Pflichtoptionen) emittiert werden.

TEST_CASE_FIXTURE(CliFixture,
                  "analyze --quiet --verbose --format console --output: mutual-exclusion vor --output-console") {
    // Plan-Praezedenz Stufe 1 (Mutual-Exclusion) muss vor Stufe 3
    // (--output mit --format console) greifen. stderr enthaelt nur die
    // Mutual-Exclusion-Meldung, nicht die --output-Fehlermeldung.
    const TemporaryDirectory temp_dir;
    const auto target = (temp_dir.path() / "report.txt").string();
    CHECK(run({"analyze", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
               "--verbose", "--format", "console", "--output", target.c_str()}) ==
          ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("--quiet and --verbose are mutually exclusive") != std::string::npos);
    CHECK(err.str().find("--output is not supported with --format console") == std::string::npos);
    CHECK(err.str().find("verbose:") == std::string::npos);
    CHECK_FALSE(std::filesystem::exists(target));
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --quiet --verbose without --changed-file: mutual-exclusion vor Pflichtoption") {
    // Plan-Praezedenz Stufe 1 vor Stufe 4. stderr enthaelt nur den
    // Mutual-Exclusion-Fehler, nicht "impact requires --changed-file".
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
               "--verbose"}) == ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("--quiet and --verbose are mutually exclusive") != std::string::npos);
    CHECK(err.str().find("impact requires --changed-file") == std::string::npos);
    CHECK(err.str().find("verbose:") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --format console --output without --changed-file: --output-console vor Pflichtoption") {
    // Plan-Praezedenz Stufe 3 vor Stufe 4. stderr enthaelt nur den
    // --output-mit-console-Fehler, nicht "impact requires --changed-file".
    const TemporaryDirectory temp_dir;
    const auto target = (temp_dir.path() / "report.txt").string();
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--format",
               "console", "--output", target.c_str()}) == ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("--output is not supported with --format console") != std::string::npos);
    CHECK(err.str().find("impact requires --changed-file") == std::string::npos);
    CHECK_FALSE(std::filesystem::exists(target));
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --quiet --verbose --format console --output without --changed-file: Stufe 1 gewinnt vor 3 und 4") {
    // Triple-Verletzung: Mutual-Exclusion + --output console + missing
    // --changed-file. Stufe 1 (Mutual-Exclusion) muss gewinnen.
    const TemporaryDirectory temp_dir;
    const auto target = (temp_dir.path() / "report.txt").string();
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--quiet",
               "--verbose", "--format", "console", "--output", target.c_str()}) ==
          ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("--quiet and --verbose are mutually exclusive") != std::string::npos);
    CHECK(err.str().find("--output is not supported with --format console") == std::string::npos);
    CHECK(err.str().find("impact requires --changed-file") == std::string::npos);
    CHECK(err.str().find("verbose:") == std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture,
                  "impact --verbose --format json --output without --changed-file: Pflichtoption vor Verbose-Block") {
    // Plan-Praezedenz Stufe 4 (Pflichtoption) greift bevor der Verbose-
    // Fehlerkontext-Helfer aus Tranche C.1 feuern darf. stderr enthaelt
    // nur die Usage-Fehlermeldung, kein verbose:-Praefix.
    const TemporaryDirectory temp_dir;
    const auto target = (temp_dir.path() / "report.json").string();
    CHECK(run({"impact", "--compile-commands",
               fixture_path("m2/basic_project/compile_commands.json").c_str(), "--verbose",
               "--format", "json", "--output", target.c_str()}) == ExitCode::cli_usage_error);
    CHECK(out.str().empty());
    CHECK(err.str().find("impact requires --changed-file") != std::string::npos);
    CHECK(err.str().find("verbose:") == std::string::npos);
    CHECK_FALSE(std::filesystem::exists(target));
}

// ---- AP M5-1.5 Tranche D: Order-Independence ---------------------------
//
// `--quiet`/`--verbose` muessen an jeder Position hinter dem Subcommand
// akzeptiert werden. CLI11 ist von sich aus order-tolerant, aber die Tests
// pinnen das Verhalten als Vertrag fuer kuenftige Refactorings.

TEST_CASE_FIXTURE(CliFixture, "analyze --quiet position-independence: trailing flag") {
    // --quiet at the very end after --top.
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m3/report_project/compile_commands.json").c_str(), "--format",
                 "console", "--top", "2", "--quiet"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-analyze-quiet.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze --verbose position-independence: between input and format") {
    // --verbose between --compile-commands and --format.
    REQUIRE(run({"analyze", "--compile-commands",
                 fixture_path("m3/report_project/compile_commands.json").c_str(), "--verbose",
                 "--format", "console", "--top", "2"}) == ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-analyze-verbose.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "impact --quiet position-independence: leading flag") {
    // --quiet immediately after the subcommand, before any other option.
    REQUIRE(run({"impact", "--quiet", "--compile-commands",
                 fixture_path("m3/report_impact_header/compile_commands.json").c_str(),
                 "--changed-file", "include/common/config.h", "--format", "console"}) ==
            ExitCode::success);
    const auto golden =
        read_text_file(fixture_path("m5/verbosity/console-impact-quiet.txt"));
    CHECK(out.str() == golden);
    CHECK(err.str().empty());
}
