#include <doctest/doctest.h>

#include <string>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/compile_database_status.h"
#include "hexagon/ports/driven/compile_database_port.h"
#include "hexagon/ports/driven/report_writer_port.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

namespace {

class StubCompileDatabasePort final : public xray::hexagon::ports::driven::CompileDatabasePort {
public:
    xray::hexagon::model::CompileDatabaseStatus load_compile_database() const override {
        return {
            .dependency_available = true,
        };
    }
};

class StubReportWriterPort final : public xray::hexagon::ports::driven::ReportWriterPort {
public:
    std::string write_report(const xray::hexagon::model::AnalysisResult& analysis_result) const override {
        return std::string(analysis_result.application.name) + "::" +
               std::string(analysis_result.summary);
    }
};

}  // namespace

TEST_CASE("project analyzer exposes placeholder analysis via the compile database port") {
    const StubCompileDatabasePort compile_database_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_port};

    const auto analysis_result = analyzer.analyze_project();

    CHECK(analysis_result.application.name == std::string_view{"cmake-xray"});
    CHECK(analysis_result.application.version == std::string_view{"v0.1.0"});
    CHECK(analysis_result.compile_database.dependency_available);
    CHECK(analysis_result.summary == std::string_view{"placeholder binary for milestone M0"});
}

TEST_CASE("report generator delegates rendering to the report writer port") {
    const StubReportWriterPort report_writer_port;
    const xray::hexagon::services::ReportGenerator generator{report_writer_port};
    const xray::hexagon::model::AnalysisResult analysis_result{
        .application = xray::hexagon::model::application_info(),
        .compile_database = {.dependency_available = true},
        .summary = "placeholder binary for milestone M0",
    };

    CHECK(generator.generate_report(analysis_result) ==
          "cmake-xray::placeholder binary for milestone M0");
}
