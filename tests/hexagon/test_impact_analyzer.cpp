#include <doctest/doctest.h>

#include <string_view>
#include <vector>

#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/target_info.h"
#include "hexagon/ports/driven/build_model_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/services/impact_analyzer.h"

namespace {

using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::ResolvedTranslationUnitIncludes;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetMetadataStatus;

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
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/src/main.cpp", "");

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
}

TEST_CASE("impact analyzer reports heuristic header matches") {
    const StubBuildModelPort build_model_port;
    const UnusedBuildModelPort unused_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port, include_resolver_port,
                                                           unused_port};

    const auto result =
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/include/common/config.h", "");

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
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/include/common/shared.h", "");

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
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/include/generated/version.h", "");

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
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/include/common/partial.h", "");

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
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/include/common/config.h", "");

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
            analyzer.analyze_impact("", "/project/src/main.cpp", "/tmp/build");

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
            analyzer.analyze_impact("", "/project/include/common/config.h", "/tmp/build");

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
        analyzer.analyze_impact("", "/project/src/z_main.cpp", "/tmp/build");

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
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/src/main.cpp", "/tmp/build");

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
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/src/main.cpp", "/tmp/build");

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
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/src/main.cpp", "/tmp/build");

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
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/src/main.cpp", "/nonexistent");

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
        analyzer.analyze_impact("", "src/main.cpp", "/tmp/build");

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
        analyzer.analyze_impact("/project/compile_commands.json", "src/main.cpp", "/tmp/build");

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
        analyzer.analyze_impact("", "/project/src/shared.cpp", "/tmp/build");

    REQUIRE(result.affected_targets.size() == 3);
    // Sorted by (classification, display_name, type)
    CHECK(result.affected_targets[0].target.display_name == "alpha");
    CHECK(result.affected_targets[0].target.type == "SHARED_LIBRARY");
    CHECK(result.affected_targets[1].target.display_name == "alpha");
    CHECK(result.affected_targets[1].target.type == "STATIC_LIBRARY");
    CHECK(result.affected_targets[2].target.display_name == "zebra");
}
