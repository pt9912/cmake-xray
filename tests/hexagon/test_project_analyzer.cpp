#include <doctest/doctest.h>

#include <string>
#include <string_view>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/ports/driven/compile_database_port.h"
#include "hexagon/ports/driven/report_writer_port.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

namespace {

class StubCompileDatabasePort final : public xray::hexagon::ports::driven::CompileDatabasePort {
public:
    xray::hexagon::model::CompileDatabaseResult load_compile_database(
        std::string_view /*path*/) const override {
        return {
            .error = xray::hexagon::model::CompileDatabaseError::none,
            .error_description = {},
            .entries = {
                xray::hexagon::model::CompileEntry{"main.cpp", "/project", {"g++", "main.cpp"}},
            },
            .entry_diagnostics = {},
        };
    }
};

class StubReportWriterPort final : public xray::hexagon::ports::driven::ReportWriterPort {
public:
    std::string write_report(
        const xray::hexagon::model::AnalysisResult& analysis_result) const override {
        return std::string(analysis_result.application.name) + "::" +
               std::to_string(analysis_result.compile_database.entries.size()) + " entries";
    }
};

}  // namespace

TEST_CASE("project analyzer loads compile database through driven port") {
    const StubCompileDatabasePort compile_database_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_port};

    const auto result = analyzer.analyze_project("/path/to/compile_commands.json");

    CHECK(result.application.name == std::string_view{"cmake-xray"});
    CHECK(result.application.version == std::string_view{"v0.2.0"});
    CHECK(result.compile_database.is_success());
    CHECK(result.compile_database.entries.size() == 1);
    CHECK(result.compile_database.entries[0].file() == "main.cpp");
}

TEST_CASE("project analyzer propagates compile database errors") {
    class ErrorCompileDatabasePort final
        : public xray::hexagon::ports::driven::CompileDatabasePort {
    public:
        xray::hexagon::model::CompileDatabaseResult load_compile_database(
            std::string_view /*path*/) const override {
            return {
                .error = xray::hexagon::model::CompileDatabaseError::empty_database,
                .error_description = "compile_commands.json is empty",
                .entries = {},
                .entry_diagnostics = {},
            };
        }
    };

    const ErrorCompileDatabasePort compile_database_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_port};

    const auto result = analyzer.analyze_project("/path/to/compile_commands.json");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error ==
          xray::hexagon::model::CompileDatabaseError::empty_database);
}

TEST_CASE("report generator delegates rendering to the report writer port") {
    const StubReportWriterPort report_writer_port;
    const xray::hexagon::services::ReportGenerator generator{report_writer_port};
    const xray::hexagon::model::AnalysisResult analysis_result{
        .application = xray::hexagon::model::application_info(),
        .compile_database = {
            .error = xray::hexagon::model::CompileDatabaseError::none,
            .error_description = {},
            .entries = {
                xray::hexagon::model::CompileEntry{"main.cpp", "/project", {"g++", "main.cpp"}},
            },
            .entry_diagnostics = {},
        },
    };

    CHECK(generator.generate_report(analysis_result) == "cmake-xray::1 entries");
}
