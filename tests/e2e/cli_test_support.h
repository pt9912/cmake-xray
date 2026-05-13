#pragma once

// Shared test support for the AP M5-1.5 split test suites in this directory.
// test_cli.cpp, test_cli_render_errors.cpp and test_cli_verbosity.cpp all
// include this header so the CliAdapter wiring, the StubAnalyzeProjectPort
// family and the helper classes only live in one place. doctest TEST_CASE
// macros expand to types whose names must be visible at expansion time, so
// every helper that participates in a TEST_CASE_FIXTURE is defined here.

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "adapters/cli/atomic_report_writer.h"
#include "adapters/cli/cli_adapter.h"
#include "adapters/cli/cli_report_renderer.h"
#include "adapters/cli/exit_codes.h"
#include "adapters/input/cmake_file_api_adapter.h"
#include "adapters/input/analysis_json_reader.h"
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
#include "hexagon/services/compare_service.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

namespace xray::tests::cli_support {

using xray::adapters::cli::CliAdapter;
using xray::adapters::cli::ExitCode;
using xray::adapters::input::CmakeFileApiAdapter;
using xray::adapters::input::AnalysisJsonReader;
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
using xray::hexagon::services::CompareService;
using xray::hexagon::services::ProjectAnalyzer;
using xray::hexagon::services::ReportGenerator;

inline const std::string testdata = "tests/e2e/testdata/";

inline std::string fixture_path(std::string_view relative_path) {
    return testdata + std::string(relative_path);
}

inline std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>{input},
                       std::istreambuf_iterator<char>{});
}

inline bool contains_temporary_report_file(const std::filesystem::path& directory) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().filename().string().starts_with(".cmake-xray-")) return true;
    }
    return false;
}

class TemporaryDirectory {
public:
    TemporaryDirectory() : path_(make_unique_path()) {
        std::filesystem::create_directories(path_);
    }

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

class StubGenerateReportPort final : public xray::hexagon::ports::driving::GenerateReportPort {
public:
    std::string generate_analysis_report(const AnalysisResult&,
                                         std::size_t) const override {
        return {};
    }

    std::string generate_impact_report(const ImpactResult&) const override { return {}; }
};

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

struct CliFixture {
    CompileCommandsJsonAdapter compile_database_adapter;
    CmakeFileApiAdapter file_api_adapter;
    SourceParsingIncludeAdapter include_resolver_adapter;
    AnalysisJsonReader analysis_json_reader;
    ConsoleReportAdapter console_report_adapter;
    MarkdownReportAdapter markdown_report_adapter;
    JsonReportAdapter json_report_adapter;
    DotReportAdapter dot_report_adapter;
    HtmlReportAdapter html_report_adapter;
    ProjectAnalyzer project_analyzer{compile_database_adapter, include_resolver_adapter,
                                     file_api_adapter};
    ImpactAnalyzer impact_analyzer{compile_database_adapter, include_resolver_adapter,
                                   file_api_adapter};
    CompareService compare_service{analysis_json_reader};
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
    CliAdapter cli{project_analyzer, impact_analyzer, compare_service, report_ports};
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

inline AnalysisResult make_analyze_success_result() {
    AnalysisResult r;
    r.application = xray::hexagon::model::application_info();
    r.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    return r;
}

inline ImpactResult make_impact_success_result() {
    ImpactResult r;
    r.application = xray::hexagon::model::application_info();
    r.compile_database = CompileDatabaseResult{CompileDatabaseError::none, {}, {}, {}};
    r.inputs.changed_file = std::string{"src/app/main.cpp"};
    r.inputs.changed_file_source =
        xray::hexagon::model::ChangedFileSource::compile_database_directory;
    return r;
}

}  // namespace xray::tests::cli_support
