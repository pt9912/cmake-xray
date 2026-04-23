#include <doctest/doctest.h>

#include <string_view>
#include <vector>

#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/ports/driven/build_model_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/services/impact_analyzer.h"

namespace {

using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::ResolvedTranslationUnitIncludes;

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
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port,
                                                           include_resolver_port};

    const auto result =
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/src/main.cpp", "");

    CHECK(result.compile_database.is_success());
    CHECK(result.compile_database_path == "/tmp/compile_commands.json");
    CHECK_FALSE(result.heuristic);
    REQUIRE(result.affected_translation_units.size() == 2);
    CHECK(result.affected_translation_units[0].kind == ImpactKind::direct);
    CHECK(result.affected_translation_units[1].kind == ImpactKind::direct);
    CHECK(result.affected_translation_units[0].reference.directory == "/project/build/debug");
    CHECK(result.affected_translation_units[1].reference.directory == "/project/build/release");
    CHECK(result.diagnostics.empty());
}

TEST_CASE("impact analyzer reports heuristic header matches") {
    const StubBuildModelPort build_model_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port,
                                                           include_resolver_port};

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
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port,
                                                           include_resolver_port};

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
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port,
                                                           include_resolver_port};

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
    const PartialIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port,
                                                           include_resolver_port};

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
    const SortingIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{build_model_port,
                                                           include_resolver_port};

    const auto result =
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/include/common/config.h", "");

    REQUIRE(result.diagnostics.size() == 4);
    CHECK(result.diagnostics[0].message == "alpha warning");
    CHECK(result.diagnostics[1].message == "middle warning");
    CHECK(result.diagnostics[2].message.find("generated includes") != std::string::npos);
    CHECK(result.diagnostics[3].message == "zebra note");
}
