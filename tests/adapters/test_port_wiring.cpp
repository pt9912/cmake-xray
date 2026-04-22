#include <doctest/doctest.h>

#include <string_view>

#include "adapters/cli/placeholder_cli_adapter.h"
#include "adapters/input/json_dependency_probe.h"
#include "adapters/output/placeholder_report_adapter.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/application_info.h"
#include "hexagon/ports/driving/analyze_project_port.h"
#include "hexagon/ports/driving/generate_report_port.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

namespace {

class StubAnalyzeProjectPort final : public xray::hexagon::ports::driving::AnalyzeProjectPort {
public:
    xray::hexagon::model::AnalysisResult analyze_project(
        std::string_view /*compile_commands_path*/) const override {
        return {
            .application = xray::hexagon::model::application_info(),
            .compile_database = {},
        };
    }
};

class StubGenerateReportPort final : public xray::hexagon::ports::driving::GenerateReportPort {
public:
    std::string generate_report(
        const xray::hexagon::model::AnalysisResult& analysis_result) const override {
        return std::string(analysis_result.application.name) + " via CLI";
    }
};

}  // namespace

TEST_CASE("driven adapters satisfy their hexagon port interfaces") {
    const xray::adapters::input::JsonDependencyProbe compile_database_adapter;
    const xray::adapters::output::PlaceholderReportAdapter report_adapter;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_adapter};
    const xray::hexagon::services::ReportGenerator generator{report_adapter};

    const auto analysis_result = analyzer.analyze_project("");

    CHECK(analysis_result.compile_database.is_success());
}

TEST_CASE("cli adapter drives the hexagon through primary ports") {
    const StubAnalyzeProjectPort analyze_project_port;
    const StubGenerateReportPort generate_report_port;
    const xray::adapters::cli::PlaceholderCliAdapter cli_adapter{
        analyze_project_port,
        generate_report_port,
    };

    CHECK(cli_adapter.description() == "cmake-xray placeholder CLI adapter");
    CHECK(cli_adapter.run() == "cmake-xray via CLI");
}
