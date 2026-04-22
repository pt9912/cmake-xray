#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/ports/driven/compile_database_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/ports/driven/report_writer_port.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

namespace {

using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::ResolvedTranslationUnitIncludes;

std::vector<CompileEntry> stub_entries() {
    return {
        CompileEntry::from_arguments(
            "/project/src/main.cpp", "/project/build/app",
            {"clang++", "-I/project/include", "-iquote/project/src", "-DAPP=1", "-c",
             "/project/src/main.cpp", "-o", "main.o"}),
        CompileEntry::from_command("/project/src/core.cpp", "/project/build/lib",
                                   "clang++ -I/project/include -c /project/src/core.cpp -o core.o"),
        CompileEntry::from_arguments("/project/src/tool.cpp", "/project/build/tools",
                                     {"clang++", "-I/project/include", "-DTOOL=1", "-c",
                                      "/project/src/tool.cpp", "-o", "tool.o"}),
    };
}

std::vector<CompileEntry> permuted_stub_entries() {
    return {
        CompileEntry::from_arguments("/project/src/tool.cpp", "/project/build/tools",
                                     {"clang++", "-I/project/include", "-DTOOL=1", "-c",
                                      "/project/src/tool.cpp", "-o", "tool.o"}),
        CompileEntry::from_arguments(
            "/project/src/main.cpp", "/project/build/app",
            {"clang++", "-I/project/include", "-iquote/project/src", "-DAPP=1", "-c",
             "/project/src/main.cpp", "-o", "main.o"}),
        CompileEntry::from_command("/project/src/core.cpp", "/project/build/lib",
                                   "clang++ -I/project/include -c /project/src/core.cpp -o core.o"),
    };
}

class StubCompileDatabasePort final : public xray::hexagon::ports::driven::CompileDatabasePort {
public:
    explicit StubCompileDatabasePort(std::vector<CompileEntry> entries = stub_entries())
        : entries_(std::move(entries)) {}

    CompileDatabaseResult load_compile_database(std::string_view /*path*/) const override {
        return CompileDatabaseResult{CompileDatabaseError::none, {}, entries_, {}};
    }

private:
    std::vector<CompileEntry> entries_;
};

class StubIncludeResolverPort final : public xray::hexagon::ports::driven::IncludeResolverPort {
public:
    IncludeResolutionResult resolve_includes(
        const std::vector<xray::hexagon::model::TranslationUnitObservation>& translation_units)
        const override {
        if (translation_units.size() != 3) return {};

        return IncludeResolutionResult{
            .heuristic = true,
            .translation_units =
                {
                    ResolvedTranslationUnitIncludes{
                        translation_units[0].reference.unique_key,
                        {"/project/include/common/config.h", "/project/include/common/shared.h"},
                        {{xray::hexagon::model::DiagnosticSeverity::warning,
                          "could not resolve include \"generated/version.h\" from /project/src/main.cpp"}},
                    },
                    ResolvedTranslationUnitIncludes{
                        translation_units[1].reference.unique_key,
                        {"/project/include/common/config.h", "/project/include/common/shared.h"},
                        {},
                    },
                    ResolvedTranslationUnitIncludes{
                        translation_units[2].reference.unique_key,
                        {"/project/include/common/config.h", "/project/include/common/shared.h"},
                        {},
                    },
                },
            .diagnostics =
                {
                    {xray::hexagon::model::DiagnosticSeverity::note, "zebra note"},
                    {xray::hexagon::model::DiagnosticSeverity::warning, "alpha warning"},
                    {xray::hexagon::model::DiagnosticSeverity::warning, "alpha warning"},
                },
        };
    }
};

class StubReportWriterPort final : public xray::hexagon::ports::driven::ReportWriterPort {
public:
    std::string write_analysis_report(
        const xray::hexagon::model::AnalysisResult& analysis_result,
        std::size_t top_limit) const override {
        return std::string(analysis_result.application.name) + "::" +
               std::to_string(top_limit) + "::" +
               std::to_string(analysis_result.translation_units.size()) + " translation units";
    }

    std::string write_impact_report(
        const xray::hexagon::model::ImpactResult& impact_result) const override {
        return impact_result.changed_file + "::" +
               std::to_string(impact_result.affected_translation_units.size());
    }
};

class EmptyIncludeResolverPort final : public xray::hexagon::ports::driven::IncludeResolverPort {
public:
    IncludeResolutionResult resolve_includes(
        const std::vector<xray::hexagon::model::TranslationUnitObservation>&) const override {
        return {};
    }
};

}  // namespace

TEST_CASE("project analyzer builds ranked translation units and hotspots") {
    const StubCompileDatabasePort compile_database_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_port,
                                                            include_resolver_port};

    const auto result = analyzer.analyze_project("/tmp/compile_commands.json");

    CHECK(result.application.name == std::string_view{"cmake-xray"});
    CHECK(result.application.version == std::string_view{"v1.0.0"});
    CHECK(result.compile_database.is_success());
    CHECK(result.compile_database_path == "/tmp/compile_commands.json");
    REQUIRE(result.translation_units.size() == 3);
    CHECK(result.translation_units[0].reference.source_path == "/project/src/main.cpp");
    CHECK(result.translation_units[1].reference.source_path == "/project/src/tool.cpp");
    CHECK(result.translation_units[2].reference.source_path == "/project/src/core.cpp");
    CHECK(result.translation_units[0].score() > result.translation_units[1].score());
    REQUIRE(result.translation_units[0].diagnostics.size() == 1);
    CHECK(result.translation_units[0].diagnostics[0].message.find("generated/version.h") !=
          std::string::npos);

    CHECK(result.include_analysis_heuristic);
    REQUIRE(result.include_hotspots.size() == 2);
    CHECK(result.include_hotspots[0].header_path == "/project/include/common/config.h");
    CHECK(result.include_hotspots[0].affected_translation_units.size() == 3);
    CHECK(result.include_hotspots[1].header_path == "/project/include/common/shared.h");
    REQUIRE(result.diagnostics.size() == 3);
    CHECK(result.diagnostics[0].message == "alpha warning");
    CHECK(result.diagnostics[1].message.find("heuristic") != std::string::npos);
    CHECK(result.diagnostics[2].message == "zebra note");
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

    class UnusedIncludeResolverPort final : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>&) const override {
            return {};
        }
    };

    const ErrorCompileDatabasePort compile_database_port;
    const UnusedIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_port,
                                                            include_resolver_port};

    const auto result = analyzer.analyze_project("/path/to/compile_commands.json");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::empty_database);
    CHECK(result.translation_units.empty());
}

TEST_CASE("project analyzer ranking is stable for permuted compile database entries") {
    const StubCompileDatabasePort baseline_compile_database_port;
    const StubCompileDatabasePort permuted_compile_database_port{permuted_stub_entries()};
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer baseline_analyzer{baseline_compile_database_port,
                                                                     include_resolver_port};
    const xray::hexagon::services::ProjectAnalyzer permuted_analyzer{permuted_compile_database_port,
                                                                     include_resolver_port};

    const auto baseline_result = baseline_analyzer.analyze_project("/tmp/compile_commands.json");
    const auto permuted_result = permuted_analyzer.analyze_project("/tmp/compile_commands.json");

    REQUIRE(baseline_result.translation_units.size() == 3);
    REQUIRE(permuted_result.translation_units.size() == 3);

    for (std::size_t index = 0; index < baseline_result.translation_units.size(); ++index) {
        CHECK(permuted_result.translation_units[index].reference.unique_key ==
              baseline_result.translation_units[index].reference.unique_key);
        CHECK(permuted_result.translation_units[index].rank ==
              baseline_result.translation_units[index].rank);
    }
}

TEST_CASE("project analyzer tokenizes quoted command arguments with spaces") {
    const StubCompileDatabasePort compile_database_port{{CompileEntry::from_command(
        "/project/src/main file.cpp", "/project/build",
        "clang++ -I\"/project/include path\" -iquote '/project/src quoted' -DMODE=fast -c "
        "'/project/src/main file.cpp'")}};
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_port,
                                                            include_resolver_port};

    const auto result = analyzer.analyze_project("/tmp/compile_commands.json");

    REQUIRE(result.translation_units.size() == 1);
    CHECK(result.translation_units[0].arg_count == 7);
    CHECK(result.translation_units[0].include_path_count == 2);
    CHECK(result.translation_units[0].define_count == 1);
    CHECK(result.translation_units[0].diagnostics.empty());
}

TEST_CASE("project analyzer keeps best effort metrics for unmatched command quotes") {
    const StubCompileDatabasePort compile_database_port{{CompileEntry::from_command(
        "/project/src/main.cpp", "/project/build",
        "clang++ -I/project/include -DNAME=\"unterminated -c /project/src/main.cpp")}};
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_database_port,
                                                            include_resolver_port};

    const auto result = analyzer.analyze_project("/tmp/compile_commands.json");

    REQUIRE(result.translation_units.size() == 1);
    CHECK(result.translation_units[0].arg_count == 3);
    CHECK(result.translation_units[0].include_path_count == 1);
    CHECK(result.translation_units[0].define_count == 1);
    REQUIRE(result.translation_units[0].diagnostics.size() == 1);
    CHECK(result.translation_units[0].diagnostics[0].message.find("unmatched quote") !=
          std::string::npos);
}

TEST_CASE("report generator delegates rendering to the report writer port") {
    const StubReportWriterPort report_writer_port;
    const xray::hexagon::services::ReportGenerator generator{report_writer_port};
    xray::hexagon::model::AnalysisResult analysis_result;
    analysis_result.application = xray::hexagon::model::application_info();
    analysis_result.compile_database =
        CompileDatabaseResult{CompileDatabaseError::none, {}, stub_entries(), {}};
    analysis_result.translation_units = {xray::hexagon::model::RankedTranslationUnit{}};
    xray::hexagon::model::ImpactResult impact_result;
    impact_result.application = xray::hexagon::model::application_info();
    impact_result.changed_file = "include/common/config.h";

    CHECK(generator.generate_analysis_report(analysis_result, 7) ==
          "cmake-xray::7::1 translation units");
    CHECK(generator.generate_impact_report(impact_result) == "include/common/config.h::0");
}
