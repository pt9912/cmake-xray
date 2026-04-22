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

using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::CompileEntry;

class StubCompileDatabasePort final : public xray::hexagon::ports::driven::CompileDatabasePort {
public:
    CompileDatabaseResult load_compile_database(std::string_view /*path*/) const override {
        return CompileDatabaseResult{
            CompileDatabaseError::none, {},
            {CompileEntry{"main.cpp", "/project", {"g++", "main.cpp"}}}, {}};
    }
};

class StubReportWriterPort final : public xray::hexagon::ports::driven::ReportWriterPort {
public:
    std::string write_report(
        const xray::hexagon::model::AnalysisResult& analysis_result) const override {
        return std::string(analysis_result.application.name) + "::" +
               std::to_string(analysis_result.compile_database.entries().size()) + " entries";
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
    CHECK(result.compile_database.entries().size() == 1);
    CHECK(result.compile_database.entries()[0].file() == "main.cpp");
}

TEST_CASE("project analyzer propagates compile database errors") {
    class ErrorCompileDatabasePort final
        : public xray::hexagon::ports::driven::CompileDatabasePort {
    public:
        CompileDatabaseResult load_compile_database(std::string_view /*path*/) const override {
            return CompileDatabaseResult{CompileDatabaseError::empty_database,
                                         "compile_commands.json is empty", {}, {}};
        }
    };

    const ErrorCompileDatabasePort compile_database_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_port};

    const auto result = analyzer.analyze_project("/path/to/compile_commands.json");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::empty_database);
}

TEST_CASE("project analyzer propagates entry diagnostics from driven port") {
    class InvalidEntriesPort final
        : public xray::hexagon::ports::driven::CompileDatabasePort {
    public:
        CompileDatabaseResult load_compile_database(std::string_view /*path*/) const override {
            return CompileDatabaseResult{
                CompileDatabaseError::invalid_entries,
                "2 invalid entries",
                {},
                {xray::hexagon::model::EntryDiagnostic{0, "missing \"file\" field"},
                 xray::hexagon::model::EntryDiagnostic{3, "missing \"command\" and \"arguments\""}},
                2};
        }
    };

    const InvalidEntriesPort compile_database_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_port};

    const auto result = analyzer.analyze_project("/path/to/compile_commands.json");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::invalid_entries);
    CHECK(result.compile_database.total_invalid_entries() == 2);
    REQUIRE(result.compile_database.entry_diagnostics().size() == 2);
    CHECK(result.compile_database.entry_diagnostics()[0].index() == 0);
    CHECK(result.compile_database.entry_diagnostics()[1].index() == 3);
}

TEST_CASE("report generator delegates rendering to the report writer port") {
    const StubReportWriterPort report_writer_port;
    const xray::hexagon::services::ReportGenerator generator{report_writer_port};
    const xray::hexagon::model::AnalysisResult analysis_result{
        .application = xray::hexagon::model::application_info(),
        .compile_database = CompileDatabaseResult{
            CompileDatabaseError::none, {},
            {CompileEntry{"main.cpp", "/project", {"g++", "main.cpp"}}}, {}},
    };

    CHECK(generator.generate_report(analysis_result) == "cmake-xray::1 entries");
}
