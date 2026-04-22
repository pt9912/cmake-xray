#include <doctest/doctest.h>

#include <string_view>
#include <vector>

#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/ports/driven/compile_database_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/services/impact_analyzer.h"

namespace {

using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::CompileDatabaseResult;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::ResolvedTranslationUnitIncludes;

class StubCompileDatabasePort final : public xray::hexagon::ports::driven::CompileDatabasePort {
public:
    CompileDatabaseResult load_compile_database(std::string_view /*path*/) const override {
        return CompileDatabaseResult{
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
                        {},
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
    const StubCompileDatabasePort compile_database_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_database_port,
                                                           include_resolver_port};

    const auto result =
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/src/main.cpp");

    CHECK(result.compile_database.is_success());
    CHECK_FALSE(result.heuristic);
    REQUIRE(result.affected_translation_units.size() == 2);
    CHECK(result.affected_translation_units[0].kind == ImpactKind::direct);
    CHECK(result.affected_translation_units[1].kind == ImpactKind::direct);
    CHECK(result.affected_translation_units[0].reference.directory == "/project/build/debug");
    CHECK(result.affected_translation_units[1].reference.directory == "/project/build/release");
}

TEST_CASE("impact analyzer reports heuristic header matches") {
    const StubCompileDatabasePort compile_database_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_database_port,
                                                           include_resolver_port};

    const auto result =
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/include/common/config.h");

    CHECK(result.heuristic);
    REQUIRE(result.affected_translation_units.size() == 3);
    CHECK(result.affected_translation_units[0].kind == ImpactKind::heuristic);
    CHECK(result.changed_file == "/project/include/common/config.h");
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics.back().message.find("generated includes") != std::string::npos);
}

TEST_CASE("impact analyzer reports heuristic matches for transitively included headers") {
    const StubCompileDatabasePort compile_database_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_database_port,
                                                           include_resolver_port};

    const auto result =
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/include/common/shared.h");

    CHECK(result.heuristic);
    REQUIRE(result.affected_translation_units.size() == 3);
    CHECK(result.affected_translation_units[0].kind == ImpactKind::heuristic);
    CHECK(result.changed_file == "/project/include/common/shared.h");
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics.back().message.find("generated includes") != std::string::npos);
}

TEST_CASE("impact analyzer reports missing matches as heuristic empty result") {
    const StubCompileDatabasePort compile_database_port;
    const StubIncludeResolverPort include_resolver_port;
    const xray::hexagon::services::ImpactAnalyzer analyzer{compile_database_port,
                                                           include_resolver_port};

    const auto result =
        analyzer.analyze_impact("/tmp/compile_commands.json", "/project/include/generated/version.h");

    CHECK(result.heuristic);
    CHECK(result.affected_translation_units.empty());
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics.back().message.find("not present") != std::string::npos);
}
