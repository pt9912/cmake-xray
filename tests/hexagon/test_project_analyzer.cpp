#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/target_info.h"
#include "hexagon/ports/driven/build_model_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/ports/driven/report_writer_port.h"
#include "hexagon/ports/driving/analyze_project_port.h"
#include "hexagon/services/project_analyzer.h"
#include "hexagon/services/report_generator.h"

namespace {

using xray::hexagon::model::ChangedFileSource;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::ReportInputSource;
using xray::hexagon::model::ResolvedTranslationUnitIncludes;
using xray::hexagon::model::TargetAssignment;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;
using xray::hexagon::ports::driving::AnalyzeProjectRequest;
using xray::hexagon::ports::driving::InputPathArgument;

// POSIX rooted paths like "/repo" do not satisfy is_absolute() on Windows;
// abs_path adds the local drive prefix when targeting Windows so the same
// fixture exercises the absolute-path branches on both platforms.
inline std::filesystem::path abs_path(std::string_view posix_path) {
#ifdef _WIN32
    return std::filesystem::path{std::string{"C:"} + std::string{posix_path}};
#else
    return std::filesystem::path{posix_path};
#endif
}

AnalyzeProjectRequest make_project_request(std::string_view compile_commands,
                                            std::string_view file_api) {
    AnalyzeProjectRequest request;
    request.report_display_base = std::filesystem::path{"/"};
    if (!compile_commands.empty()) {
        std::filesystem::path p{compile_commands};
        request.compile_commands_path = InputPathArgument{p, p, !p.is_absolute()};
    }
    if (!file_api.empty()) {
        std::filesystem::path p{file_api};
        request.cmake_file_api_path = InputPathArgument{p, p, !p.is_absolute()};
    }
    return request;
}

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

class StubBuildModelPort final : public xray::hexagon::ports::driven::BuildModelPort {
public:
    explicit StubBuildModelPort(std::vector<CompileEntry> entries = stub_entries())
        : entries_(std::move(entries)) {}

    xray::hexagon::model::BuildModelResult load_build_model(
        std::string_view /*path*/) const override {
        xray::hexagon::model::BuildModelResult result;
        result.compile_database =
            CompileDatabaseResult{CompileDatabaseError::none, {}, entries_, {}};
        return result;
    }

private:
    std::vector<CompileEntry> entries_;
};

class UnusedBuildModelPort final : public xray::hexagon::ports::driven::BuildModelPort {
public:
    xray::hexagon::model::BuildModelResult load_build_model(
        std::string_view /*path*/) const override {
        xray::hexagon::model::BuildModelResult result;
        result.compile_database =
            CompileDatabaseResult{CompileDatabaseError::file_api_not_accessible,
                                  "should not be called", {}, {}};
        return result;
    }
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
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{build_model_port, include_resolver_port,
                                                            unused_port};

    const auto result = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", ""));

    CHECK(result.application.name == std::string_view{"cmake-xray"});
    // AP M5-1.6 Tranche A: app version source is now the build-time
    // XRAY_APP_VERSION_STRING define, normalised without a leading 'v'.
    CHECK(result.application.version == std::string_view{XRAY_APP_VERSION_STRING});
    CHECK(result.compile_database.is_success());
    CHECK(result.compile_database_path == "/tmp/compile_commands.json");
    REQUIRE(result.inputs.compile_database_path.has_value());
    CHECK(*result.inputs.compile_database_path == "/tmp/compile_commands.json");
    CHECK(result.inputs.compile_database_source == ReportInputSource::cli);
    CHECK_FALSE(result.inputs.cmake_file_api_path.has_value());
    CHECK_FALSE(result.inputs.cmake_file_api_resolved_path.has_value());
    CHECK(result.inputs.cmake_file_api_source == ReportInputSource::not_provided);
    CHECK_FALSE(result.inputs.changed_file.has_value());
    CHECK_FALSE(result.inputs.changed_file_source.has_value());
    CHECK(result.observation_source == ObservationSource::exact);
    CHECK(result.target_metadata == TargetMetadataStatus::not_loaded);
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
    class ErrorBuildModelPort final
        : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.compile_database =
                CompileDatabaseResult{CompileDatabaseError::empty_database,
                                      "compile_commands.json is empty", {}, {}};
            return result;
        }
    };

    class UnusedIncludeResolverPort final : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>&) const override {
            return {};
        }
    };

    const ErrorBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const UnusedIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{build_model_port, include_resolver_port,
                                                            unused_port};

    const auto result = analyzer.analyze_project(
        make_project_request("/path/to/compile_commands.json", ""));

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::empty_database);
    CHECK(result.translation_units.empty());
    REQUIRE(result.inputs.compile_database_path.has_value());
    CHECK(*result.inputs.compile_database_path == "/path/to/compile_commands.json");
    CHECK(result.inputs.compile_database_source == ReportInputSource::cli);
}

TEST_CASE("project analyzer ranking is stable for permuted compile database entries") {
    const StubBuildModelPort baseline_build_model_port;
    const StubBuildModelPort permuted_build_model_port{permuted_stub_entries()};
    const UnusedBuildModelPort unused_port;
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer baseline_analyzer{
        baseline_build_model_port, include_resolver_port, unused_port};
    const xray::hexagon::services::ProjectAnalyzer permuted_analyzer{
        permuted_build_model_port, include_resolver_port, unused_port};

    const auto baseline_result =
        baseline_analyzer.analyze_project(make_project_request("/tmp/compile_commands.json", ""));
    const auto permuted_result =
        permuted_analyzer.analyze_project(make_project_request("/tmp/compile_commands.json", ""));

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
    const StubBuildModelPort build_model_port{{CompileEntry::from_command(
        "/project/src/main file.cpp", "/project/build",
        "clang++ -I\"/project/include path\" -iquote '/project/src quoted' -DMODE=fast -c "
        "'/project/src/main file.cpp'")}};
    const UnusedBuildModelPort unused_port;
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{build_model_port, include_resolver_port,
                                                            unused_port};

    const auto result = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", ""));

    REQUIRE(result.translation_units.size() == 1);
    CHECK(result.translation_units[0].arg_count == 7);
    CHECK(result.translation_units[0].include_path_count == 2);
    CHECK(result.translation_units[0].define_count == 1);
    CHECK(result.translation_units[0].diagnostics.empty());
}

TEST_CASE("project analyzer keeps best effort metrics for unmatched command quotes") {
    const StubBuildModelPort build_model_port{{CompileEntry::from_command(
        "/project/src/main.cpp", "/project/build",
        "clang++ -I/project/include -DNAME=\"unterminated -c /project/src/main.cpp")}};
    const UnusedBuildModelPort unused_port;
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{build_model_port, include_resolver_port,
                                                            unused_port};

    const auto result = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", ""));

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

TEST_CASE("project analyzer uses file api as derived source with targets on TUs") {
    class FileApiBuildModelPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.source = ObservationSource::derived;
            result.target_metadata = TargetMetadataStatus::loaded;
            result.source_root = "/project";
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::none, {},
                {CompileEntry::from_arguments(
                    "/project/src/main.cpp", "/project/build/app",
                    {"clang++", "-I/project/include", "-c", "/project/src/main.cpp"})},
                {}};
            result.target_assignments = {
                {"/project/src/main.cpp|/project/build/app",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}}};
            result.diagnostics = {{xray::hexagon::model::DiagnosticSeverity::note,
                                    "1 of 2 targets have no compilable sources and are not included in the analysis"}};
            return result;
        }
    };

    const UnusedBuildModelPort compile_db_port;
    const FileApiBuildModelPort file_api_port;
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                            file_api_port};

    const auto result = analyzer.analyze_project(make_project_request("", "/tmp/build"));

    CHECK(result.compile_database.is_success());
    CHECK(result.observation_source == ObservationSource::derived);
    CHECK(result.target_metadata == TargetMetadataStatus::loaded);
    REQUIRE(result.translation_units.size() == 1);
    REQUIRE(result.translation_units[0].targets.size() == 1);
    CHECK(result.translation_units[0].targets[0].display_name == "myapp");
    REQUIRE(result.target_assignments.size() == 1);
    CHECK(result.diagnostics.size() >= 1);
    CHECK_FALSE(result.inputs.compile_database_path.has_value());
    CHECK(result.inputs.compile_database_source == ReportInputSource::not_provided);
    REQUIRE(result.inputs.cmake_file_api_path.has_value());
    CHECK(*result.inputs.cmake_file_api_path == "/tmp/build");
    CHECK(result.inputs.cmake_file_api_source == ReportInputSource::cli);
}

TEST_CASE("project analyzer filters file api assignments and attaches targets in mixed path") {
    class FileApiWithPartialMatch final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.source = ObservationSource::derived;
            result.target_metadata = TargetMetadataStatus::loaded;
            result.source_root = "/project";
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::none, {},
                {CompileEntry::from_arguments(
                    "/project/src/main.cpp", "/project/build/app",
                    {"clang++", "-I/project/include", "-c", "/project/src/main.cpp"})},
                {}};
            // One assignment matches the compile DB, one does not
            result.target_assignments = {
                {"/project/src/extra.cpp|/project/build/app",
                 {{"extra", "STATIC_LIBRARY", "extra::STATIC_LIBRARY"}}},
                {"/project/src/main.cpp|/project/build/app",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}}};
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const FileApiWithPartialMatch file_api_port;
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                            file_api_port};

    const auto result = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", "/tmp/build"));

    CHECK(result.compile_database.is_success());
    CHECK(result.observation_source == ObservationSource::exact);
    CHECK(result.target_metadata == TargetMetadataStatus::loaded);
    REQUIRE(result.translation_units.size() == 3);
    // Only the matching assignment survives filtering
    REQUIRE(result.target_assignments.size() == 1);
    CHECK(result.target_assignments[0].targets[0].display_name == "myapp");
    // Target is attached to the matching TU
    bool found_tu_with_target = false;
    for (const auto& tu : result.translation_units) {
        if (!tu.targets.empty()) {
            CHECK(tu.targets[0].display_name == "myapp");
            found_tu_with_target = true;
        }
    }
    CHECK(found_tu_with_target);
    // Unmatched observations diagnostic
    bool found_unmatched_diag = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.message.find("1 of 2 file api observations have no match") !=
            std::string::npos) {
            found_unmatched_diag = true;
        }
    }
    CHECK(found_unmatched_diag);
    // Partial target coverage diagnostic
    bool found_coverage_diag = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.message.find("of 3 translation units have target assignments") !=
            std::string::npos) {
            found_coverage_diag = true;
        }
    }
    CHECK(found_coverage_diag);
}

TEST_CASE("project analyzer propagates file api error in mixed path") {
    class ErrorFileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.compile_database =
                CompileDatabaseResult{CompileDatabaseError::file_api_invalid,
                                      "cmake file api index is not valid JSON", {}, {}};
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const ErrorFileApiPort file_api_port;
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                            file_api_port};

    const auto result = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", "/tmp/build"));

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.translation_units.empty());
}

TEST_CASE("project analyzer reports mismatch diagnostic when no file api assignments match") {
    class MismatchedFileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.source = ObservationSource::derived;
            result.target_metadata = TargetMetadataStatus::loaded;
            result.source_root = "/other";
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::none, {},
                {CompileEntry::from_arguments(
                    "/other/src/foo.cpp", "/other/build",
                    {"clang++", "-c", "/other/src/foo.cpp"})},
                {}};
            result.target_assignments = {
                {"/other/src/foo.cpp|/other/build",
                 {{"otherapp", "EXECUTABLE", "otherapp::EXECUTABLE"}}}};
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const MismatchedFileApiPort file_api_port;
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                            file_api_port};

    const auto result = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", "/tmp/build"));

    CHECK(result.compile_database.is_success());
    CHECK(result.observation_source == ObservationSource::exact);
    REQUIRE(result.translation_units.size() == 3);
    CHECK(result.target_assignments.empty());

    bool found_mismatch = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.message.find("no file api target assignment matches") != std::string::npos) {
            found_mismatch = true;
            break;
        }
    }
    CHECK(found_mismatch);
}

TEST_CASE("project analyzer carries resolved file api path from build model into ReportInputs") {
    const auto repo_root = abs_path("/repo");
    const auto resolved_reply = abs_path("/repo/build/.cmake/api/v1/reply");
    class ResolvedPathFileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        explicit ResolvedPathFileApiPort(std::filesystem::path resolved)
            : resolved_(std::move(resolved)) {}
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.source = ObservationSource::derived;
            result.target_metadata = TargetMetadataStatus::loaded;
            result.source_root = "/project";
            result.cmake_file_api_resolved_path = resolved_;
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::none, {},
                {CompileEntry::from_arguments(
                    "/project/src/main.cpp", "/project/build/app",
                    {"clang++", "-c", "/project/src/main.cpp"})},
                {}};
            return result;
        }
    private:
        std::filesystem::path resolved_;
    };

    const UnusedBuildModelPort compile_db_port;
    const ResolvedPathFileApiPort file_api_port{resolved_reply};
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                            file_api_port};

    AnalyzeProjectRequest request;
    request.report_display_base = repo_root;
    request.cmake_file_api_path =
        InputPathArgument{std::filesystem::path{"build"}, repo_root / "build", true};

    const auto result = analyzer.analyze_project(request);

    CHECK(result.compile_database.is_success());
    REQUIRE(result.inputs.cmake_file_api_path.has_value());
    CHECK(*result.inputs.cmake_file_api_path == "build");
    REQUIRE(result.inputs.cmake_file_api_resolved_path.has_value());
    CHECK(*result.inputs.cmake_file_api_resolved_path == "build/.cmake/api/v1/reply");
}

TEST_CASE("project analyzer preserves ReportInputs on compile database load failure") {
    class ErrorBuildModelPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.compile_database =
                CompileDatabaseResult{CompileDatabaseError::file_not_accessible,
                                      "cannot read", {}, {}};
            return result;
        }
    };

    const ErrorBuildModelPort compile_db_port;
    const UnusedBuildModelPort file_api_port;
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                            file_api_port};

    const auto result = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", ""));

    CHECK_FALSE(result.compile_database.is_success());
    REQUIRE(result.inputs.compile_database_path.has_value());
    CHECK(*result.inputs.compile_database_path == "/tmp/compile_commands.json");
    CHECK(result.inputs.compile_database_source == ReportInputSource::cli);
    CHECK_FALSE(result.inputs.cmake_file_api_path.has_value());
}

TEST_CASE("project analyzer preserves resolved file api path on post-resolution failure") {
    class PostResolutionErrorPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.cmake_file_api_resolved_path =
                std::filesystem::path{"/tmp/build/.cmake/api/v1/reply"};
            result.compile_database =
                CompileDatabaseResult{CompileDatabaseError::file_api_invalid,
                                      "cmake file api index is not valid JSON", {}, {}};
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const PostResolutionErrorPort file_api_port;
    const EmptyIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                            file_api_port};

    const auto result = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", "/tmp/build"));

    CHECK_FALSE(result.compile_database.is_success());
    REQUIRE(result.inputs.cmake_file_api_path.has_value());
    CHECK(*result.inputs.cmake_file_api_path == "/tmp/build");
    REQUIRE(result.inputs.cmake_file_api_resolved_path.has_value());
    CHECK(*result.inputs.cmake_file_api_resolved_path == "/tmp/build/.cmake/api/v1/reply");
}

TEST_CASE("project analyzer is stable when the process working directory changes") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ProjectAnalyzer analyzer{build_model_port, include_resolver_port,
                                                            unused_port};

    const auto baseline = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", ""));

    const auto previous_cwd = std::filesystem::current_path();
    std::filesystem::current_path(std::filesystem::temp_directory_path());
    const auto after_cwd_change = analyzer.analyze_project(
        make_project_request("/tmp/compile_commands.json", ""));
    std::filesystem::current_path(previous_cwd);

    REQUIRE(baseline.translation_units.size() == after_cwd_change.translation_units.size());
    for (std::size_t i = 0; i < baseline.translation_units.size(); ++i) {
        CHECK(baseline.translation_units[i].reference.unique_key ==
              after_cwd_change.translation_units[i].reference.unique_key);
    }
}
