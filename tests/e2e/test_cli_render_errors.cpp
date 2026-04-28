#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "cli_test_support.h"

using namespace xray::tests::cli_support;

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
