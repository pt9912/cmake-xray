#include <doctest/doctest.h>

#include <filesystem>
#include <string_view>
#include <vector>

#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"
#include "hexagon/ports/driven/build_model_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/ports/driving/analyze_impact_port.h"
#include "hexagon/services/impact_analyzer.h"

namespace {

using xray::hexagon::model::ChangedFileSource;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::ReportInputSource;
using xray::hexagon::model::ResolvedTranslationUnitIncludes;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetMetadataStatus;
using xray::hexagon::ports::driving::AnalyzeImpactRequest;
using xray::hexagon::ports::driving::InputPathArgument;

// POSIX rooted paths like "/repo" do not satisfy is_absolute() on Windows;
// abs_path adds the local drive prefix when the build targets Windows so the
// same fixture exercises the absolute-path branches on both platforms.
inline std::filesystem::path abs_path(std::string_view posix_path) {
#ifdef _WIN32
    return std::filesystem::path{std::string{"C:"} + std::string{posix_path}};
#else
    return std::filesystem::path{posix_path};
#endif
}

AnalyzeImpactRequest make_impact_request(std::string_view compile_commands,
                                          std::string_view changed_file,
                                          std::string_view file_api) {
    AnalyzeImpactRequest request;
    request.report_display_base = std::filesystem::path{"/"};
    {
        std::filesystem::path p{changed_file};
        request.changed_file_path = InputPathArgument{p, p, !p.is_absolute()};
    }
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

class StubBuildModelPort final : public xray::hexagon::ports::driven::BuildModelPort {
public:
    xray::hexagon::model::BuildModelResult load_build_model(
        std::string_view /*path*/) const override {
        xray::hexagon::model::BuildModelResult result;
        result.compile_database = CompileDatabaseResult{
            CompileDatabaseError::none,
            {},
            {
                CompileEntry::from_arguments("/project/src/main.cpp", "/project/build/debug",
                                             {"clang++", "-I/project/include", "-c",
                                              "/project/src/main.cpp"}),
                CompileEntry::from_arguments("/project/src/main.cpp", "/project/build/release",
                                             {"clang++", "-I/project/include", "-DREL=1", "-c",
                                              "/project/src/main.cpp"}),
                CompileEntry::from_arguments("/project/src/core.cpp", "/project/build/core",
                                             {"clang++", "-I/project/include", "-c",
                                              "/project/src/core.cpp"}),
            },
            {},
        };
        return result;
    }
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
        return IncludeResolutionResult{
            .heuristic = true,
            .translation_units =
                {
                    ResolvedTranslationUnitIncludes{
                        translation_units[0].reference.unique_key,
                        {"/project/include/common/config.h",
                         "/project/include/common/shared.h"},
                        {{xray::hexagon::model::DiagnosticSeverity::warning,
                          "could not resolve include \"generated/version.h\" from /project/src/main.cpp"}},
                    },
                    ResolvedTranslationUnitIncludes{
                        translation_units[1].reference.unique_key,
                        {"/project/include/common/config.h",
                         "/project/include/common/shared.h"},
                        {},
                    },
                    ResolvedTranslationUnitIncludes{
                        translation_units[2].reference.unique_key,
                        {"/project/include/common/config.h",
                         "/project/include/common/shared.h"},
                        {},
                    },
                },
            .diagnostics = {},
        };
    }
};

}  // namespace

TEST_CASE("impact analyzer reports direct matches for duplicate translation-unit observations") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port, include_resolver_port,
                                                           unused_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/src/main.cpp", ""));

    CHECK(result.compile_database.is_success());
    CHECK(result.compile_database_path == "/tmp/compile_commands.json");
    CHECK(result.observation_source == ObservationSource::exact);
    CHECK_FALSE(result.heuristic);
    REQUIRE(result.affected_translation_units.size() == 2);
    CHECK(result.affected_translation_units[0].kind == ImpactKind::direct);
    CHECK(result.affected_translation_units[1].kind == ImpactKind::direct);
    CHECK(result.affected_translation_units[0].reference.directory == "/project/build/debug");
    CHECK(result.affected_translation_units[1].reference.directory == "/project/build/release");
    CHECK(result.diagnostics.empty());
    CHECK(result.affected_targets.empty());
    REQUIRE(result.inputs.compile_database_path.has_value());
    CHECK(*result.inputs.compile_database_path == "/tmp/compile_commands.json");
    CHECK(result.inputs.compile_database_source == ReportInputSource::cli);
    CHECK_FALSE(result.inputs.cmake_file_api_path.has_value());
    REQUIRE(result.inputs.changed_file.has_value());
    CHECK(*result.inputs.changed_file == "/project/src/main.cpp");
    REQUIRE(result.inputs.changed_file_source.has_value());
    // POSIX treats "/project/..." as absolute (cli_absolute provenance);
    // Windows treats it as drive-less and falls back to the compile DB
    // directory because the fixture path is resolved against the parent of
    // the compile_commands.json the test passed in.
#ifdef _WIN32
    CHECK(*result.inputs.changed_file_source == ChangedFileSource::compile_database_directory);
#else
    CHECK(*result.inputs.changed_file_source == ChangedFileSource::cli_absolute);
#endif
}

TEST_CASE("impact analyzer reports heuristic header matches") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port, include_resolver_port,
                                                           unused_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/include/common/config.h", ""));

    CHECK(result.heuristic);
    REQUIRE(result.affected_translation_units.size() == 3);
    CHECK(result.affected_translation_units[0].kind == ImpactKind::heuristic);
    CHECK(result.changed_file == "/project/include/common/config.h");
    REQUIRE(result.diagnostics.size() == 2);
    CHECK(result.diagnostics[0].message.find("generated/version.h") != std::string::npos);
    CHECK(result.diagnostics.back().message.find("generated includes") != std::string::npos);
}

TEST_CASE("impact analyzer reports heuristic matches for transitively included headers") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port, include_resolver_port,
                                                           unused_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/include/common/shared.h", ""));

    CHECK(result.heuristic);
    REQUIRE(result.affected_translation_units.size() == 3);
    CHECK(result.affected_translation_units[0].kind == ImpactKind::heuristic);
    CHECK(result.changed_file == "/project/include/common/shared.h");
    REQUIRE(result.diagnostics.size() == 2);
    CHECK(result.diagnostics[0].message.find("generated/version.h") != std::string::npos);
    CHECK(result.diagnostics.back().message.find("generated includes") != std::string::npos);
}

TEST_CASE("impact analyzer reports missing matches as heuristic empty result") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port, include_resolver_port,
                                                           unused_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/include/generated/version.h", ""));

    CHECK(result.heuristic);
    CHECK(result.affected_translation_units.empty());
    REQUIRE(result.diagnostics.size() == 3);
    CHECK(result.diagnostics[0].message.find("generated/version.h") != std::string::npos);
    CHECK(result.diagnostics[1].message.find("generated includes") != std::string::npos);
    CHECK(result.diagnostics[2].message.find("not present") != std::string::npos);
}

TEST_CASE("impact analyzer only keeps diagnostics for impacted translation units") {
    class PartialIncludeResolverPort final : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>& translation_units)
            const override {
            return IncludeResolutionResult{
                .heuristic = true,
                .translation_units =
                    {
                        ResolvedTranslationUnitIncludes{
                            translation_units[0].reference.unique_key,
                            {"/project/include/common/partial.h"},
                            {{xray::hexagon::model::DiagnosticSeverity::warning,
                              "could not resolve include \"generated/partial.h\" from /project/src/main.cpp"}},
                        },
                        ResolvedTranslationUnitIncludes{
                            translation_units[1].reference.unique_key,
                            {},
                            {{xray::hexagon::model::DiagnosticSeverity::warning,
                              "could not resolve include \"generated/release.h\" from /project/src/main.cpp"}},
                        },
                        ResolvedTranslationUnitIncludes{
                            translation_units[2].reference.unique_key,
                            {},
                            {{xray::hexagon::model::DiagnosticSeverity::warning,
                              "could not resolve include \"generated/core.h\" from /project/src/core.cpp"}},
                        },
                    },
                .diagnostics = {},
            };
        }
    };

    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const PartialIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port, include_resolver_port,
                                                           unused_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/include/common/partial.h", ""));

    CHECK(result.heuristic);
    REQUIRE(result.affected_translation_units.size() == 1);
    REQUIRE(result.diagnostics.size() >= 2);
    CHECK(result.diagnostics[0].message.find("generated/partial.h") != std::string::npos);
    CHECK(result.diagnostics[0].message.find("generated/release.h") == std::string::npos);
    CHECK(result.diagnostics[0].message.find("generated/core.h") == std::string::npos);
}

TEST_CASE("impact analyzer sorts report-wide diagnostics deterministically") {
    class SortingIncludeResolverPort final : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>& translation_units)
            const override {
            return IncludeResolutionResult{
                .heuristic = true,
                .translation_units =
                    {
                        ResolvedTranslationUnitIncludes{
                            translation_units[0].reference.unique_key,
                            {"/project/include/common/config.h"},
                            {{xray::hexagon::model::DiagnosticSeverity::warning, "middle warning"}},
                        },
                        ResolvedTranslationUnitIncludes{
                            translation_units[1].reference.unique_key,
                            {},
                            {},
                        },
                        ResolvedTranslationUnitIncludes{
                            translation_units[2].reference.unique_key,
                            {},
                            {},
                        },
                    },
                .diagnostics =
                    {
                        {xray::hexagon::model::DiagnosticSeverity::note, "zebra note"},
                        {xray::hexagon::model::DiagnosticSeverity::warning, "alpha warning"},
                    },
            };
        }
    };

    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const SortingIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port, include_resolver_port,
                                                           unused_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/include/common/config.h", ""));

    REQUIRE(result.diagnostics.size() == 4);
    CHECK(result.diagnostics[0].message == "alpha warning");
    CHECK(result.diagnostics[1].message == "middle warning");
    CHECK(result.diagnostics[2].message.find("generated includes") != std::string::npos);
    CHECK(result.diagnostics[3].message == "zebra note");
}

TEST_CASE("impact analyzer computes affected targets from file api data") {
    class TargetBuildModelPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.source = ObservationSource::derived;
            result.target_metadata = TargetMetadataStatus::loaded;
            result.source_root = "/project";
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::none, {},
                {
                    CompileEntry::from_arguments(
                        "/project/src/main.cpp", "/project/build/app",
                        {"clang++", "-I/project/include", "-c", "/project/src/main.cpp"}),
                    CompileEntry::from_arguments(
                        "/project/src/core.cpp", "/project/build/lib",
                        {"clang++", "-I/project/include", "-c", "/project/src/core.cpp"}),
                },
                {}};
            result.target_assignments = {
                {"/project/src/core.cpp|/project/build/lib",
                 {{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"}}},
                {"/project/src/main.cpp|/project/build/app",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}},
            };
            return result;
        }
    };

    class SimpleIncludeResolverPort final
        : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>& translation_units)
            const override {
            return IncludeResolutionResult{
                .heuristic = true,
                .translation_units =
                    {
                        ResolvedTranslationUnitIncludes{
                            translation_units[0].reference.unique_key,
                            {"/project/include/common/config.h"},
                            {},
                        },
                        ResolvedTranslationUnitIncludes{
                            translation_units[1].reference.unique_key,
                            {"/project/include/common/config.h"},
                            {},
                        },
                    },
                .diagnostics = {},
            };
        }
    };

    const UnusedBuildModelPort compile_db_port;
    const TargetBuildModelPort file_api_port;
    const SimpleIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    SUBCASE("direct impact attaches targets to impacted TU") {
        const auto result =
            analyzer.analyze_impact(make_impact_request("", "/project/src/main.cpp", "/tmp/build"));

        CHECK(result.observation_source == ObservationSource::derived);
        CHECK(result.target_metadata == TargetMetadataStatus::loaded);
        REQUIRE(result.affected_translation_units.size() == 1);
        CHECK(result.affected_translation_units[0].kind == ImpactKind::direct);
        REQUIRE(result.affected_translation_units[0].targets.size() == 1);
        CHECK(result.affected_translation_units[0].targets[0].display_name == "myapp");
        REQUIRE(result.affected_targets.size() == 1);
        CHECK(result.affected_targets[0].target.display_name == "myapp");
        CHECK(result.affected_targets[0].classification == TargetImpactClassification::direct);
    }

    SUBCASE("heuristic impact attaches targets to all impacted TUs") {
        const auto result =
            analyzer.analyze_impact(make_impact_request("", "/project/include/common/config.h", "/tmp/build"));

        CHECK(result.heuristic);
        REQUIRE(result.affected_translation_units.size() == 2);
        for (const auto& tu : result.affected_translation_units) {
            REQUIRE(tu.targets.size() == 1);
        }
        REQUIRE(result.affected_targets.size() == 2);
        CHECK(result.affected_targets[0].classification == TargetImpactClassification::heuristic);
        CHECK(result.affected_targets[1].classification == TargetImpactClassification::heuristic);
    }
}

TEST_CASE("impact analyzer promotes heuristic target to direct when later observation is direct") {
    // a_helper.cpp sorts before z_main.cpp, so heuristic assignment is processed first.
    // When z_main.cpp (direct) is processed, myapp must be promoted from heuristic to direct.
    class DirectWinsBuildModelPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.source = ObservationSource::derived;
            result.target_metadata = TargetMetadataStatus::loaded;
            result.source_root = "/project";
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::none, {},
                {
                    CompileEntry::from_arguments(
                        "/project/src/a_helper.cpp", "/project/build/app",
                        {"clang++", "-I/project/include", "-c", "/project/src/a_helper.cpp"}),
                    CompileEntry::from_arguments(
                        "/project/src/z_main.cpp", "/project/build/app",
                        {"clang++", "-I/project/include", "-c", "/project/src/z_main.cpp"}),
                },
                {}};
            result.target_assignments = {
                {"/project/src/a_helper.cpp|/project/build/app",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}},
                {"/project/src/z_main.cpp|/project/build/app",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}},
            };
            return result;
        }
    };

    class HelperIncludeResolverPort final
        : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>& translation_units)
            const override {
            return IncludeResolutionResult{
                .heuristic = true,
                .translation_units =
                    {
                        ResolvedTranslationUnitIncludes{
                            translation_units[0].reference.unique_key,
                            {"/project/src/z_main.cpp"},
                            {},
                        },
                        ResolvedTranslationUnitIncludes{
                            translation_units[1].reference.unique_key,
                            {},
                            {},
                        },
                    },
                .diagnostics = {},
            };
        }
    };

    const UnusedBuildModelPort compile_db_port;
    const DirectWinsBuildModelPort file_api_port;
    const HelperIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    // z_main.cpp is the changed file: direct impact on z_main.cpp, heuristic on a_helper.cpp.
    // Both map to myapp. a_helper (heuristic) sorts first. Direct must win.
    const auto result =
        analyzer.analyze_impact(make_impact_request("", "/project/src/z_main.cpp", "/tmp/build"));

    REQUIRE(result.affected_targets.size() == 1);
    CHECK(result.affected_targets[0].target.display_name == "myapp");
    CHECK(result.affected_targets[0].classification == TargetImpactClassification::direct);
}

TEST_CASE("impact analyzer enriches compile database with file api targets in mixed path") {
    class FileApiTargetPort final : public xray::hexagon::ports::driven::BuildModelPort {
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
                    "/project/src/main.cpp", "/project/build/debug",
                    {"clang++", "-c", "/project/src/main.cpp"})},
                {}};
            result.target_assignments = {
                {"/project/src/main.cpp|/project/build/debug",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}},
            };
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const FileApiTargetPort file_api_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/src/main.cpp", "/tmp/build"));

    CHECK(result.compile_database.is_success());
    CHECK(result.observation_source == ObservationSource::exact);
    CHECK(result.target_metadata == TargetMetadataStatus::loaded);
    REQUIRE(result.affected_translation_units.size() == 2);
    REQUIRE(result.affected_targets.size() == 1);
    CHECK(result.affected_targets[0].target.display_name == "myapp");
    CHECK(result.affected_targets[0].classification == TargetImpactClassification::direct);
}

TEST_CASE("impact analyzer filters file api assignments and reports unmatched observations") {
    class PartialMatchFileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
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
                    "/project/src/main.cpp", "/project/build/debug",
                    {"clang++", "-c", "/project/src/main.cpp"})},
                {}};
            // One matches (main.cpp|debug), one does not (extra.cpp|debug)
            result.target_assignments = {
                {"/project/src/extra.cpp|/project/build/debug",
                 {{"extra", "STATIC_LIBRARY", "extra::STATIC_LIBRARY"}}},
                {"/project/src/main.cpp|/project/build/debug",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}},
            };
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const PartialMatchFileApiPort file_api_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/src/main.cpp", "/tmp/build"));

    CHECK(result.compile_database.is_success());
    // Only the matching assignment survives
    REQUIRE(result.target_assignments.size() == 1);
    CHECK(result.target_assignments[0].targets[0].display_name == "myapp");
    // Unmatched diagnostic
    bool found_unmatched_diag = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.message.find("1 of 2 file api observations have no match") !=
            std::string::npos) {
            found_unmatched_diag = true;
        }
    }
    CHECK(found_unmatched_diag);
    // Target is attached to the impacted TU
    REQUIRE(result.affected_translation_units.size() == 2);
    bool found_tu_with_target = false;
    for (const auto& tu : result.affected_translation_units) {
        if (!tu.targets.empty()) {
            CHECK(tu.targets[0].display_name == "myapp");
            found_tu_with_target = true;
        }
    }
    CHECK(found_tu_with_target);
}

TEST_CASE("impact analyzer reports mismatch diagnostic when no file api assignments match") {
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
                 {{"otherapp", "EXECUTABLE", "otherapp::EXECUTABLE"}}},
            };
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const MismatchedFileApiPort file_api_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/src/main.cpp", "/tmp/build"));

    CHECK(result.compile_database.is_success());
    CHECK(result.observation_source == ObservationSource::exact);

    bool found_mismatch = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.message.find("no file api target assignment matches") != std::string::npos) {
            found_mismatch = true;
            break;
        }
    }
    CHECK(found_mismatch);
    CHECK(result.target_assignments.empty());
    CHECK(result.affected_targets.empty());
}

TEST_CASE("impact analyzer propagates file api error in mixed path") {
    class ErrorFileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.compile_database =
                CompileDatabaseResult{CompileDatabaseError::file_api_not_accessible,
                                      "cannot access cmake file api reply directory", {}, {}};
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const ErrorFileApiPort file_api_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("/tmp/compile_commands.json", "/project/src/main.cpp", "/nonexistent"));

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_not_accessible);
}

TEST_CASE("impact analyzer resolves relative changed file via source root in file-api-only path") {
    class FileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.source = ObservationSource::derived;
            result.target_metadata = TargetMetadataStatus::loaded;
            result.source_root = "/project";
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::none, {},
                {
                    CompileEntry::from_arguments(
                        "/project/src/main.cpp", "/project/build/app",
                        {"clang++", "-I/project/include", "-c", "/project/src/main.cpp"}),
                    CompileEntry::from_arguments(
                        "/project/src/core.cpp", "/project/build/lib",
                        {"clang++", "-I/project/include", "-c", "/project/src/core.cpp"}),
                },
                {}};
            result.target_assignments = {
                {"/project/src/main.cpp|/project/build/app",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}},
                {"/project/src/core.cpp|/project/build/lib",
                 {{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"}}},
            };
            return result;
        }
    };

    class DirectIncludeResolver final : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>&) const override {
            return {};
        }
    };

    const UnusedBuildModelPort compile_db_port;
    const FileApiPort file_api_port;
    const DirectIncludeResolver include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    // Relative path "src/main.cpp" must resolve against source_root "/project"
    const auto result =
        analyzer.analyze_impact(make_impact_request("", "src/main.cpp", "/tmp/build"));

    CHECK(result.observation_source == ObservationSource::derived);
    CHECK(result.changed_file == "src/main.cpp");
    CHECK_FALSE(result.heuristic);
    REQUIRE(result.affected_translation_units.size() == 1);
    CHECK(result.affected_translation_units[0].kind == ImpactKind::direct);
    CHECK(result.affected_translation_units[0].reference.source_path == "src/main.cpp");
    REQUIRE(result.affected_translation_units[0].targets.size() == 1);
    CHECK(result.affected_translation_units[0].targets[0].display_name == "myapp");
    REQUIRE(result.affected_targets.size() == 1);
    CHECK(result.affected_targets[0].target.display_name == "myapp");
    CHECK(result.affected_targets[0].classification == TargetImpactClassification::direct);
}

TEST_CASE("impact analyzer resolves relative changed file via compile database directory in mixed path") {
    // Compile DB at /project/compile_commands.json → base directory is /project
    // Entries use absolute paths /project/src/*.cpp
    // File API provides target assignments
    // Relative changed_path "src/main.cpp" must resolve to /project/src/main.cpp

    class MixedFileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
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
                    "/project/src/main.cpp", "/project/build/debug",
                    {"clang++", "-c", "/project/src/main.cpp"})},
                {}};
            result.target_assignments = {
                {"/project/src/main.cpp|/project/build/debug",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}},
                {"/project/src/core.cpp|/project/build/core",
                 {{"core", "STATIC_LIBRARY", "core::STATIC_LIBRARY"}}},
            };
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const MixedFileApiPort file_api_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    // Relative path "src/main.cpp" must resolve against /tmp (parent of /tmp/compile_commands.json)
    // StubBuildModelPort entries use absolute paths /project/src/main.cpp
    // Since /tmp/src/main.cpp != /project/src/main.cpp, we need a path that resolves correctly.
    // Use /project/compile_commands.json so base = /project, then "src/main.cpp" → /project/src/main.cpp
    const auto result =
        analyzer.analyze_impact(make_impact_request("/project/compile_commands.json", "src/main.cpp", "/tmp/build"));

    CHECK(result.observation_source == ObservationSource::exact);
    CHECK(result.target_metadata == TargetMetadataStatus::loaded);
    CHECK(result.changed_file == "src/main.cpp");
    CHECK_FALSE(result.heuristic);
    // Direct match on both duplicate observations (debug and release) from StubBuildModelPort
    REQUIRE(result.affected_translation_units.size() == 2);
    CHECK(result.affected_translation_units[0].kind == ImpactKind::direct);
    CHECK(result.affected_translation_units[1].kind == ImpactKind::direct);
    // File API target is attached via filtered assignment
    REQUIRE(result.affected_targets.size() == 1);
    CHECK(result.affected_targets[0].target.display_name == "myapp");
    CHECK(result.affected_targets[0].classification == TargetImpactClassification::direct);
}

TEST_CASE("impact analyzer sorts affected targets deterministically") {
    class MultiTargetBuildModelPort final : public xray::hexagon::ports::driven::BuildModelPort {
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
                    "/project/src/shared.cpp", "/project/build/lib",
                    {"clang++", "-c", "/project/src/shared.cpp"})},
                {}};
            result.target_assignments = {
                {"/project/src/shared.cpp|/project/build/lib",
                 {{"zebra", "STATIC_LIBRARY", "zebra::STATIC_LIBRARY"},
                  {"alpha", "SHARED_LIBRARY", "alpha::SHARED_LIBRARY"},
                  {"alpha", "STATIC_LIBRARY", "alpha::STATIC_LIBRARY"}}},
            };
            return result;
        }
    };

    class EmptyIncludeResolver final : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>&) const override {
            return {};
        }
    };

    const UnusedBuildModelPort compile_db_port;
    const MultiTargetBuildModelPort file_api_port;
    const EmptyIncludeResolver include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    const auto result =
        analyzer.analyze_impact(make_impact_request("", "/project/src/shared.cpp", "/tmp/build"));

    REQUIRE(result.affected_targets.size() == 3);
    // Sorted by (classification, display_name, type)
    CHECK(result.affected_targets[0].target.display_name == "alpha");
    CHECK(result.affected_targets[0].target.type == "SHARED_LIBRARY");
    CHECK(result.affected_targets[1].target.display_name == "alpha");
    CHECK(result.affected_targets[1].target.type == "STATIC_LIBRARY");
    CHECK(result.affected_targets[2].target.display_name == "zebra");
}

TEST_CASE("impact analyzer reports compile_database_directory provenance for relative changed file with compile DB") {
    class MixedFileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
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
                    "/project/src/main.cpp", "/project/build/debug",
                    {"clang++", "-c", "/project/src/main.cpp"})},
                {}};
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const MixedFileApiPort file_api_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    const auto result = analyzer.analyze_impact(
        make_impact_request("/project/compile_commands.json", "src/main.cpp", "/tmp/build"));

    REQUIRE(result.inputs.changed_file_source.has_value());
    CHECK(*result.inputs.changed_file_source == ChangedFileSource::compile_database_directory);
    REQUIRE(result.inputs.changed_file.has_value());
    CHECK(*result.inputs.changed_file == "src/main.cpp");
}

TEST_CASE("impact analyzer reports file_api_source_root provenance for relative changed file with file API only") {
    class FileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
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
                    {"clang++", "-c", "/project/src/main.cpp"})},
                {}};
            return result;
        }
    };

    class EmptyIncludeResolver final : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>&) const override {
            return {};
        }
    };

    const UnusedBuildModelPort compile_db_port;
    const FileApiPort file_api_port;
    const EmptyIncludeResolver include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    const auto result = analyzer.analyze_impact(
        make_impact_request("", "src/main.cpp", "/tmp/build"));

    REQUIRE(result.inputs.changed_file_source.has_value());
    CHECK(*result.inputs.changed_file_source == ChangedFileSource::file_api_source_root);
    REQUIRE(result.inputs.changed_file.has_value());
    CHECK(*result.inputs.changed_file == "src/main.cpp");
}

TEST_CASE("impact analyzer reports unresolved_file_api_source_root when file API load fails before source root is known") {
    class FailingFileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::file_api_invalid,
                "cmake file api index is not valid JSON", {}, {}};
            return result;
        }
    };

    class EmptyIncludeResolver final : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>&) const override {
            return {};
        }
    };

    const UnusedBuildModelPort compile_db_port;
    const FailingFileApiPort file_api_port;
    const EmptyIncludeResolver include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    const auto result = analyzer.analyze_impact(
        make_impact_request("", "src/main.cpp", "/tmp/build"));

    CHECK_FALSE(result.compile_database.is_success());
    REQUIRE(result.inputs.changed_file_source.has_value());
    CHECK(*result.inputs.changed_file_source ==
          ChangedFileSource::unresolved_file_api_source_root);
    REQUIRE(result.inputs.changed_file.has_value());
    CHECK(*result.inputs.changed_file == "src/main.cpp");
    REQUIRE(result.inputs.cmake_file_api_path.has_value());
    CHECK(*result.inputs.cmake_file_api_path == "/tmp/build");
    CHECK_FALSE(result.inputs.compile_database_path.has_value());
}

TEST_CASE("impact analyzer carries resolved file api path from build model into ReportInputs") {
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

    class EmptyIncludeResolver final : public xray::hexagon::ports::driven::IncludeResolverPort {
    public:
        IncludeResolutionResult resolve_includes(
            const std::vector<xray::hexagon::model::TranslationUnitObservation>&) const override {
            return {};
        }
    };

    const UnusedBuildModelPort compile_db_port;
    const ResolvedPathFileApiPort file_api_port{resolved_reply};
    const EmptyIncludeResolver include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    AnalyzeImpactRequest request;
    request.report_display_base = repo_root;
    request.cmake_file_api_path =
        InputPathArgument{std::filesystem::path{"build"}, repo_root / "build", true};
    request.changed_file_path =
        InputPathArgument{abs_path("/project/src/main.cpp"),
                          abs_path("/project/src/main.cpp"), false};

    const auto result = analyzer.analyze_impact(request);

    REQUIRE(result.inputs.cmake_file_api_resolved_path.has_value());
    CHECK(*result.inputs.cmake_file_api_resolved_path == "build/.cmake/api/v1/reply");
}

TEST_CASE("impact analyzer preserves ReportInputs on compile database load failure") {
    class ErrorCompileDbPort final : public xray::hexagon::ports::driven::BuildModelPort {
    public:
        xray::hexagon::model::BuildModelResult load_build_model(
            std::string_view /*path*/) const override {
            xray::hexagon::model::BuildModelResult result;
            result.compile_database = CompileDatabaseResult{
                CompileDatabaseError::file_not_accessible, "cannot read", {}, {}};
            return result;
        }
    };

    const ErrorCompileDbPort compile_db_port;
    const UnusedBuildModelPort file_api_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    const auto result = analyzer.analyze_impact(
        make_impact_request("/tmp/compile_commands.json", "src/main.cpp", ""));

    CHECK_FALSE(result.compile_database.is_success());
    REQUIRE(result.inputs.compile_database_path.has_value());
    CHECK(*result.inputs.compile_database_path == "/tmp/compile_commands.json");
    REQUIRE(result.inputs.changed_file_source.has_value());
    CHECK(*result.inputs.changed_file_source == ChangedFileSource::compile_database_directory);
}

TEST_CASE("impact analyzer keeps changed_file relative when path_for_io is pre-resolved absolute") {
    // Plan rule: when was_relative == true, services must not treat path_for_io as
    // a fachlich aufgeloesten Impact-Pfad. Simulate a CLI that pre-resolved
    // path_for_io to an absolute form: changed_file must still come from
    // original_argument relative to the compile DB directory.
    class FixedFileApiPort final : public xray::hexagon::ports::driven::BuildModelPort {
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
                    "/project/src/main.cpp", "/project/build/debug",
                    {"clang++", "-c", "/project/src/main.cpp"})},
                {}};
            result.target_assignments = {
                {"/project/src/main.cpp|/project/build/debug",
                 {{"myapp", "EXECUTABLE", "myapp::EXECUTABLE"}}}};
            return result;
        }
    };

    const StubBuildModelPort compile_db_port;
    const FixedFileApiPort file_api_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_db_port, include_resolver_port,
                                                           file_api_port};

    AnalyzeImpactRequest request;
    request.report_display_base = std::filesystem::path{"/"};
    request.compile_commands_path =
        InputPathArgument{std::filesystem::path{"/project/compile_commands.json"},
                          std::filesystem::path{"/project/compile_commands.json"}, false};
    // Pre-resolved absolute path_for_io with was_relative=true.
    request.changed_file_path =
        InputPathArgument{std::filesystem::path{"src/main.cpp"},
                          std::filesystem::path{"/project/src/main.cpp"}, true};
    request.cmake_file_api_path =
        InputPathArgument{std::filesystem::path{"/tmp/build"},
                          std::filesystem::path{"/tmp/build"}, false};

    const auto result = analyzer.analyze_impact(request);

    REQUIRE(result.inputs.changed_file_source.has_value());
    CHECK(*result.inputs.changed_file_source == ChangedFileSource::compile_database_directory);
    REQUIRE(result.inputs.changed_file.has_value());
    CHECK(*result.inputs.changed_file == "src/main.cpp");
    CHECK(result.changed_file == "src/main.cpp");
}

// --- M6 AP 1.1 A.4: target-graph propagation in ImpactAnalyzer ---

namespace m6_ia {

using xray::hexagon::model::TargetAssignment;
using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetGraphStatus;
using xray::hexagon::model::TargetInfo;

TargetGraph two_target_graph() {
    TargetGraph g;
    g.nodes.push_back(TargetInfo{"a", "STATIC_LIBRARY", "a::STATIC_LIBRARY"});
    g.nodes.push_back(TargetInfo{"b", "STATIC_LIBRARY", "b::STATIC_LIBRARY"});
    g.edges.push_back(TargetDependency{"a::STATIC_LIBRARY", "a",
                                       "b::STATIC_LIBRARY", "b",
                                       TargetDependencyKind::direct,
                                       TargetDependencyResolution::resolved});
    return g;
}

class FileApiPortWithGraph final
    : public xray::hexagon::ports::driven::BuildModelPort {
public:
    FileApiPortWithGraph(TargetGraph graph, TargetGraphStatus status,
                         std::vector<TargetAssignment> assignments = {})
        : graph_(std::move(graph)),
          status_(status),
          assignments_(std::move(assignments)) {}

    xray::hexagon::model::BuildModelResult load_build_model(
        std::string_view) const override {
        xray::hexagon::model::BuildModelResult result;
        result.source = ObservationSource::derived;
        result.target_metadata = TargetMetadataStatus::loaded;
        result.source_root = "/project";
        result.compile_database = CompileDatabaseResult{
            CompileDatabaseError::none, {},
            {CompileEntry::from_arguments("/project/src/main.cpp",
                                          "/project/build/debug",
                                          {"clang++", "-I/project/include", "-c",
                                           "/project/src/main.cpp"}),
             CompileEntry::from_arguments("/project/src/main.cpp",
                                          "/project/build/release",
                                          {"clang++", "-I/project/include", "-DREL=1",
                                           "-c", "/project/src/main.cpp"}),
             CompileEntry::from_arguments("/project/src/core.cpp",
                                          "/project/build/core",
                                          {"clang++", "-I/project/include", "-c",
                                           "/project/src/core.cpp"})},
            {}};
        result.target_assignments = assignments_;
        result.target_graph = graph_;
        result.target_graph_status = status_;
        return result;
    }

private:
    TargetGraph graph_;
    TargetGraphStatus status_;
    std::vector<TargetAssignment> assignments_;
};

}  // namespace m6_ia

TEST_CASE("impact analyzer: compile-database-only path leaves target_graph at not_loaded defaults") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        build_model_port, include_resolver_port, unused_port};

    const auto result = analyzer.analyze_impact(make_impact_request(
        "/tmp/compile_commands.json", "/project/src/main.cpp", ""));

    CHECK(result.target_graph_status == m6_ia::TargetGraphStatus::not_loaded);
    CHECK(result.target_graph.nodes.empty());
    CHECK(result.target_graph.edges.empty());
}

TEST_CASE("impact analyzer: file-api-only path propagates target_graph and status from build model") {
    const m6_ia::FileApiPortWithGraph file_api_port{m6_ia::two_target_graph(),
                                                    m6_ia::TargetGraphStatus::loaded};
    const UnusedBuildModelPort compile_db_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        compile_db_port, include_resolver_port, file_api_port};

    const auto result = analyzer.analyze_impact(
        make_impact_request("", "/project/src/main.cpp", "/tmp/build"));

    REQUIRE(result.compile_database.is_success());
    CHECK(result.target_graph_status == m6_ia::TargetGraphStatus::loaded);
    REQUIRE(result.target_graph.nodes.size() == 2);
    REQUIRE(result.target_graph.edges.size() == 1);
    CHECK(result.target_graph.edges[0].from_unique_key == "a::STATIC_LIBRARY");
    CHECK(result.target_graph.edges[0].to_unique_key == "b::STATIC_LIBRARY");
}

TEST_CASE("impact analyzer: mixed-input path propagates target_graph from file-api model") {
    const m6_ia::FileApiPortWithGraph file_api_port{
        m6_ia::two_target_graph(), m6_ia::TargetGraphStatus::loaded,
        {{"/project/src/main.cpp|/project/build/debug",
          {{"a", "STATIC_LIBRARY", "a::STATIC_LIBRARY"}}}}};
    const StubBuildModelPort compile_db_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        compile_db_port, include_resolver_port, file_api_port};

    const auto result = analyzer.analyze_impact(make_impact_request(
        "/tmp/compile_commands.json", "/project/src/main.cpp", "/tmp/build"));

    REQUIRE(result.compile_database.is_success());
    CHECK(result.target_graph_status == m6_ia::TargetGraphStatus::loaded);
    REQUIRE(result.target_graph.nodes.size() == 2);
    REQUIRE(result.target_graph.edges.size() == 1);
    // affected_targets still follows M4/M5 rules: AP 1.1 does not change how
    // changed-file -> assignments -> targets resolves; the new target_graph
    // is metadata that AP 1.3 will consume for reverse-BFS prioritisation.
    REQUIRE(result.affected_targets.size() == 1);
    CHECK(result.affected_targets[0].target.unique_key == "a::STATIC_LIBRARY");
}

// ---- AP M6-1.3 A.1: service validation + reverse-BFS prioritisation ----

namespace m6_ia_a1 {

using xray::hexagon::model::TargetEvidenceStrength;
using xray::hexagon::model::TargetPriorityClass;

}  // namespace m6_ia_a1

TEST_CASE("AP1.3 A.1: impact_target_depth=33 is rejected with impact_target_depth_out_of_range") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        build_model_port, include_resolver_port, unused_port};

    auto request = make_impact_request("/tmp/compile_commands.json",
                                        "/project/src/main.cpp", "");
    request.impact_target_depth = 33;

    const auto result = analyzer.analyze_impact(request);

    REQUIRE(result.service_validation_error.has_value());
    CHECK(result.service_validation_error->code ==
          "impact_target_depth_out_of_range");
    CHECK(result.service_validation_error->message ==
          "impact_target_depth: value exceeds maximum 32");
    // No partial result: every analysis-bearing field stays at its
    // default-initialized state.
    CHECK(result.affected_translation_units.empty());
    CHECK(result.affected_targets.empty());
    CHECK(result.prioritized_affected_targets.empty());
    CHECK(result.diagnostics.empty());
}

TEST_CASE("AP1.3 A.1: require_target_graph + not_loaded is rejected with target_graph_required") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        build_model_port, include_resolver_port, unused_port};

    auto request = make_impact_request("/tmp/compile_commands.json",
                                        "/project/src/main.cpp", "");
    request.require_target_graph = true;
    request.impact_target_depth = 2;

    const auto result = analyzer.analyze_impact(request);

    REQUIRE(result.service_validation_error.has_value());
    CHECK(result.service_validation_error->code == "target_graph_required");
    CHECK(result.service_validation_error->message ==
          "target graph data is required but not available");
    // Validated depth is preserved on the rejected result so callers can
    // mirror the request provenance even when no analysis ran.
    CHECK(result.impact_target_depth_requested == 2);
    CHECK(result.affected_translation_units.empty());
    CHECK(result.prioritized_affected_targets.empty());
}

TEST_CASE("AP1.3 A.1: compile-DB-only impact populates depth fields without affecting v2 byte-stability") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        build_model_port, include_resolver_port, unused_port};

    const auto result = analyzer.analyze_impact(make_impact_request(
        "/tmp/compile_commands.json", "/project/src/main.cpp", ""));

    CHECK(result.target_graph_status == m6_ia::TargetGraphStatus::not_loaded);
    CHECK(result.impact_target_depth_requested == 2);  // service default
    CHECK(result.impact_target_depth_effective == 0);
    // Compile-DB-only path produces no affected_targets, so the seed
    // list is empty and there are no prioritised entries.
    CHECK(result.prioritized_affected_targets.empty());
    // AP M6-1.3 A.1 explicitly suppresses the four prioritisation
    // diagnostics until A.3 / A.4 surfaces the prioritised section in
    // the report adapters; the v2 diagnostics stay byte-stable.
    for (const auto& d : result.diagnostics) {
        CHECK(d.message.find("impact prioritisation") == std::string::npos);
        CHECK(d.message.find("reverse target graph traversal") ==
              std::string::npos);
    }
}

TEST_CASE("AP1.3 A.1: file-API-loaded impact runs reverse-BFS, prioritised list contains seeds + reverse hops") {
    // Graph: lib --> common, app --> lib (incoming-edge to lib from app
    // and to common from lib). Changed file is in lib's TU; expected
    // priority list: lib (direct, 0), app (direct_dependent, 1).
    using xray::hexagon::model::TargetAssignment;
    using xray::hexagon::model::TargetDependency;
    using xray::hexagon::model::TargetDependencyKind;
    using xray::hexagon::model::TargetDependencyResolution;
    using xray::hexagon::model::TargetGraph;
    using xray::hexagon::model::TargetInfo;
    TargetGraph graph;
    graph.nodes.push_back(TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"});
    graph.nodes.push_back(TargetInfo{"lib", "STATIC_LIBRARY", "lib::STATIC_LIBRARY"});
    graph.nodes.push_back(TargetInfo{"common", "STATIC_LIBRARY", "common::STATIC_LIBRARY"});
    graph.edges.push_back(TargetDependency{
        "lib::STATIC_LIBRARY", "lib", "common::STATIC_LIBRARY", "common",
        TargetDependencyKind::direct, TargetDependencyResolution::resolved});
    graph.edges.push_back(TargetDependency{
        "app::EXECUTABLE", "app", "lib::STATIC_LIBRARY", "lib",
        TargetDependencyKind::direct, TargetDependencyResolution::resolved});

    std::vector<TargetAssignment> assignments;
    assignments.push_back(TargetAssignment{
        "/project/src/main.cpp|/project/build/debug",
        {TargetInfo{"lib", "STATIC_LIBRARY", "lib::STATIC_LIBRARY"}}});
    const m6_ia::FileApiPortWithGraph file_api_port{
        std::move(graph), m6_ia::TargetGraphStatus::loaded,
        std::move(assignments)};
    const UnusedBuildModelPort compile_db_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        compile_db_port, include_resolver_port, file_api_port};

    auto request = make_impact_request("", "/project/src/main.cpp",
                                        "/tmp/build");
    request.impact_target_depth = 2;
    const auto result = analyzer.analyze_impact(request);

    REQUIRE_FALSE(result.service_validation_error.has_value());
    CHECK(result.target_graph_status == m6_ia::TargetGraphStatus::loaded);
    CHECK(result.impact_target_depth_requested == 2);
    // BFS finds lib (seed), then app (reverse-hop from lib via the
    // app -> lib edge). common is not reverse-reachable from lib; lib
    // depends ON common, not the other way around.
    REQUIRE(result.prioritized_affected_targets.size() == 2);
    CHECK(result.prioritized_affected_targets[0].target.unique_key ==
          "lib::STATIC_LIBRARY");
    CHECK(result.prioritized_affected_targets[0].graph_distance == 0);
    CHECK(result.prioritized_affected_targets[0].priority_class ==
          m6_ia_a1::TargetPriorityClass::direct);
    CHECK(result.prioritized_affected_targets[1].target.unique_key ==
          "app::EXECUTABLE");
    CHECK(result.prioritized_affected_targets[1].graph_distance == 1);
    CHECK(result.prioritized_affected_targets[1].priority_class ==
          m6_ia_a1::TargetPriorityClass::direct_dependent);
    CHECK(result.impact_target_depth_effective == 1);
}

TEST_CASE("AP1.3 A.1: depth=0 with loaded graph still emits seeds at distance 0 but no hops") {
    using xray::hexagon::model::TargetAssignment;
    using xray::hexagon::model::TargetDependency;
    using xray::hexagon::model::TargetDependencyKind;
    using xray::hexagon::model::TargetDependencyResolution;
    using xray::hexagon::model::TargetGraph;
    using xray::hexagon::model::TargetInfo;
    TargetGraph graph;
    graph.nodes.push_back(TargetInfo{"app", "EXECUTABLE", "app::EXECUTABLE"});
    graph.nodes.push_back(TargetInfo{"lib", "STATIC_LIBRARY", "lib::STATIC_LIBRARY"});
    graph.edges.push_back(TargetDependency{
        "app::EXECUTABLE", "app", "lib::STATIC_LIBRARY", "lib",
        TargetDependencyKind::direct, TargetDependencyResolution::resolved});

    std::vector<TargetAssignment> assignments;
    assignments.push_back(TargetAssignment{
        "/project/src/main.cpp|/project/build/debug",
        {TargetInfo{"lib", "STATIC_LIBRARY", "lib::STATIC_LIBRARY"}}});
    const m6_ia::FileApiPortWithGraph file_api_port{
        std::move(graph), m6_ia::TargetGraphStatus::loaded,
        std::move(assignments)};
    const UnusedBuildModelPort compile_db_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        compile_db_port, include_resolver_port, file_api_port};

    auto request = make_impact_request("", "/project/src/main.cpp",
                                        "/tmp/build");
    request.impact_target_depth = 0;
    const auto result = analyzer.analyze_impact(request);

    REQUIRE(result.prioritized_affected_targets.size() == 1);
    CHECK(result.prioritized_affected_targets[0].target.unique_key ==
          "lib::STATIC_LIBRARY");
    CHECK(result.prioritized_affected_targets[0].graph_distance == 0);
    CHECK(result.impact_target_depth_effective == 0);
}

TEST_CASE("AP1.3 A.4: partial target graph emits the partial-prioritisation warning alongside the BFS result") {
    using xray::hexagon::model::TargetAssignment;
    using xray::hexagon::model::TargetDependency;
    using xray::hexagon::model::TargetDependencyKind;
    using xray::hexagon::model::TargetDependencyResolution;
    using xray::hexagon::model::TargetGraph;
    using xray::hexagon::model::TargetInfo;
    TargetGraph graph;
    graph.nodes.push_back(TargetInfo{"lib", "STATIC_LIBRARY", "lib::STATIC_LIBRARY"});
    graph.edges.push_back(TargetDependency{
        "lib::STATIC_LIBRARY", "lib", "<external>::ghost", "ghost",
        TargetDependencyKind::direct, TargetDependencyResolution::external});

    std::vector<TargetAssignment> assignments;
    assignments.push_back(TargetAssignment{
        "/project/src/main.cpp|/project/build/debug",
        {TargetInfo{"lib", "STATIC_LIBRARY", "lib::STATIC_LIBRARY"}}});
    const m6_ia::FileApiPortWithGraph file_api_port{
        std::move(graph), m6_ia::TargetGraphStatus::partial,
        std::move(assignments)};
    const UnusedBuildModelPort compile_db_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        compile_db_port, include_resolver_port, file_api_port};

    const auto result = analyzer.analyze_impact(
        make_impact_request("", "/project/src/main.cpp", "/tmp/build"));

    CHECK(result.target_graph_status == m6_ia::TargetGraphStatus::partial);
    REQUIRE(result.prioritized_affected_targets.size() == 1);
    CHECK(result.prioritized_affected_targets[0].target.unique_key ==
          "lib::STATIC_LIBRARY");
    bool partial_seen = false;
    for (const auto& d : result.diagnostics) {
        if (d.message.find("target graph partially loaded") !=
            std::string::npos) {
            partial_seen = true;
        }
    }
    CHECK(partial_seen);
}

TEST_CASE("AP1.3 A.4: depth-limited BFS over a long chain emits the 'hit depth limit' warning") {
    using xray::hexagon::model::TargetAssignment;
    using xray::hexagon::model::TargetDependency;
    using xray::hexagon::model::TargetDependencyKind;
    using xray::hexagon::model::TargetDependencyResolution;
    using xray::hexagon::model::TargetGraph;
    using xray::hexagon::model::TargetInfo;
    TargetGraph graph;
    // Chain: hub <- d1 <- d2 <- d3 (length 3 reverse-edges).
    for (const auto& name : {"hub", "d1", "d2", "d3"}) {
        graph.nodes.push_back(TargetInfo{name, "STATIC_LIBRARY",
                                          std::string{name} + "::STATIC_LIBRARY"});
    }
    auto edge = [](std::string from_uk, std::string to_uk) {
        return TargetDependency{from_uk, from_uk.substr(0, from_uk.find("::")),
                                to_uk, to_uk.substr(0, to_uk.find("::")),
                                TargetDependencyKind::direct,
                                TargetDependencyResolution::resolved};
    };
    graph.edges.push_back(edge("d1::STATIC_LIBRARY", "hub::STATIC_LIBRARY"));
    graph.edges.push_back(edge("d2::STATIC_LIBRARY", "d1::STATIC_LIBRARY"));
    graph.edges.push_back(edge("d3::STATIC_LIBRARY", "d2::STATIC_LIBRARY"));

    std::vector<TargetAssignment> assignments;
    assignments.push_back(TargetAssignment{
        "/project/src/main.cpp|/project/build/debug",
        {TargetInfo{"hub", "STATIC_LIBRARY", "hub::STATIC_LIBRARY"}}});
    const m6_ia::FileApiPortWithGraph file_api_port{
        std::move(graph), m6_ia::TargetGraphStatus::loaded,
        std::move(assignments)};
    const UnusedBuildModelPort compile_db_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        compile_db_port, include_resolver_port, file_api_port};

    auto request =
        make_impact_request("", "/project/src/main.cpp", "/tmp/build");
    request.impact_target_depth = 2;  // chain has 3 reverse hops, budget cuts at 2
    const auto result = analyzer.analyze_impact(request);

    bool depth_limit_seen = false;
    for (const auto& d : result.diagnostics) {
        if (d.message.find("hit depth limit 2 within a cycle") !=
            std::string::npos) {
            depth_limit_seen = true;
        }
    }
    CHECK(depth_limit_seen);
    CHECK(result.impact_target_depth_effective == 2);
}

TEST_CASE("AP1.3 A.1: require_target_graph with loaded graph runs without service error") {
    using xray::hexagon::model::TargetAssignment;
    using xray::hexagon::model::TargetGraph;
    using xray::hexagon::model::TargetInfo;
    TargetGraph graph;
    graph.nodes.push_back(TargetInfo{"lib", "STATIC_LIBRARY", "lib::STATIC_LIBRARY"});
    std::vector<TargetAssignment> assignments;
    assignments.push_back(TargetAssignment{
        "/project/src/main.cpp|/project/build/debug",
        {TargetInfo{"lib", "STATIC_LIBRARY", "lib::STATIC_LIBRARY"}}});
    const m6_ia::FileApiPortWithGraph file_api_port{
        std::move(graph), m6_ia::TargetGraphStatus::loaded,
        std::move(assignments)};
    const UnusedBuildModelPort compile_db_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{
        compile_db_port, include_resolver_port, file_api_port};

    auto request = make_impact_request("", "/project/src/main.cpp",
                                        "/tmp/build");
    request.require_target_graph = true;
    const auto result = analyzer.analyze_impact(request);

    CHECK_FALSE(result.service_validation_error.has_value());
    CHECK(result.target_graph_status == m6_ia::TargetGraphStatus::loaded);
}
