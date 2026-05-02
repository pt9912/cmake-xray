#include <doctest/doctest.h>

#include <fstream>
#include <string>
#include <vector>

#include <filesystem>
#include <optional>

#include <nlohmann/json.hpp>

#include "hexagon/model/build_model_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/services/analysis_support.h"

namespace {

using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::IncludeDepthKind;
using xray::hexagon::model::IncludeEntry;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::ResolvedTranslationUnitIncludes;

// POSIX has rooted absolute paths like "/repo"; Windows requires a drive name
// (e.g. "C:/repo") for std::filesystem::path::is_absolute() to return true.
// abs_path lets the same test fixture work on both platforms.
inline std::filesystem::path abs_path(std::string_view posix_path) {
#ifdef _WIN32
    return std::filesystem::path{std::string{"C:"} + std::string{posix_path}};
#else
    return std::filesystem::path{posix_path};
#endif
}

}  // namespace

TEST_CASE("analysis support tokenizes escaped command variants and reports empty commands") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_command("/project/src/quoted.cpp", "/project/build",
                                       "clang++ \"-DNAME=a\\\"b\" -c /project/src/quoted.cpp"),
            CompileEntry::from_command(
                "/project/src/spaced.cpp", "/project/build",
                "clang++ -I/project/include\\ path -c /project/src/spaced.cpp"),
            CompileEntry::from_command("/project/src/empty.cpp", "/project/build", ""),
        },
        std::filesystem::path{"/tmp"});

    REQUIRE(observations.size() == 3);
    CHECK(observations[0].arg_count == 4);
    CHECK(observations[0].define_count == 1);
    CHECK(observations[0].diagnostics.empty());

    CHECK(observations[1].arg_count == 4);
    CHECK(observations[1].include_path_count == 1);
    CHECK(observations[1].diagnostics.empty());

    CHECK(observations[2].arg_count == 0);
    REQUIRE(observations[2].diagnostics.size() == 1);
    CHECK(observations[2].diagnostics[0].message.find("produced no arguments") !=
          std::string::npos);
}

TEST_CASE("analysis support reports missing split-flag values") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/missing-include.cpp", "/project/build",
                                         {"clang++", "-I"}),
            CompileEntry::from_arguments("/project/src/missing-define.cpp", "/project/build",
                                         {"clang++", "-D"}),
        },
        std::filesystem::path{"/tmp"});

    REQUIRE(observations.size() == 2);
    REQUIRE(observations[0].diagnostics.size() == 1);
    REQUIRE(observations[1].diagnostics.size() == 1);
    CHECK(observations[0].diagnostics[0].message.find("-I is missing a value") !=
          std::string::npos);
    CHECK(observations[1].diagnostics[0].message.find("-D is missing a value") !=
          std::string::npos);
}

TEST_CASE("analysis support ranks translation units by include path tie-breaks") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/more-includes.cpp", "/project/build",
                                         {"clang++", "-I/project/include/a",
                                          "-I/project/include/b", "-c",
                                          "/project/src/more-includes.cpp"}),
            CompileEntry::from_arguments("/project/src/more-defines.cpp", "/project/build",
                                         {"clang++", "-I/project/include/a", "-DONE=1", "-c",
                                          "/project/src/more-defines.cpp"}),
        },
        std::filesystem::path{"/tmp"});

    const auto ranked =
        xray::hexagon::services::build_ranked_translation_units(observations, {});

    REQUIRE(ranked.size() == 2);
    CHECK(ranked[0].reference.source_path == "/project/src/more-includes.cpp");
    CHECK(ranked[1].reference.source_path == "/project/src/more-defines.cpp");
}

TEST_CASE("analysis support sorts equally sized hotspots by header path") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/app.cpp", "/project/build/app",
                                         {"clang++", "-c", "/project/src/app.cpp"}),
            CompileEntry::from_arguments("/project/src/lib.cpp", "/project/build/lib",
                                         {"clang++", "-c", "/project/src/lib.cpp"}),
        },
        std::filesystem::path{"/tmp"});

    IncludeResolutionResult include_resolution;
    include_resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/project/include/b.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/a.h", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/project/include/b.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/a.h", IncludeDepthKind::direct}},
            {},
        },
    };

    const auto hotspots = xray::hexagon::services::build_include_hotspots(
                              observations, include_resolution, std::filesystem::path{"/tmp"},
                              std::filesystem::path{},
                              xray::hexagon::services::IncludeHotspotFilters{})
                              .hotspots;

    REQUIRE(hotspots.size() == 2);
    CHECK(hotspots[0].header_path == "/project/include/a.h");
    CHECK(hotspots[1].header_path == "/project/include/b.h");
}

TEST_CASE("analysis support sorts larger hotspots ahead of smaller ones") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/app.cpp", "/project/build/app",
                                         {"clang++", "-c", "/project/src/app.cpp"}),
            CompileEntry::from_arguments("/project/src/lib.cpp", "/project/build/lib",
                                         {"clang++", "-c", "/project/src/lib.cpp"}),
            CompileEntry::from_arguments("/project/src/tool.cpp", "/project/build/tool",
                                         {"clang++", "-c", "/project/src/tool.cpp"}),
        },
        std::filesystem::path{"/tmp"});

    IncludeResolutionResult include_resolution;
    include_resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/project/include/smaller.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/larger.h", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/project/include/smaller.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/larger.h", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[2].reference.unique_key,
            {IncludeEntry{"/project/include/larger.h", IncludeDepthKind::direct}},
            {},
        },
    };

    const auto hotspots = xray::hexagon::services::build_include_hotspots(
                              observations, include_resolution, std::filesystem::path{"/tmp"},
                              std::filesystem::path{},
                              xray::hexagon::services::IncludeHotspotFilters{})
                              .hotspots;

    REQUIRE(hotspots.size() == 2);
    CHECK(hotspots[0].header_path == "/project/include/larger.h");
    CHECK(hotspots[1].header_path == "/project/include/smaller.h");
}

namespace {

xray::hexagon::model::TranslationUnitObservation make_observation_with_paths(
    std::string source_path, std::vector<std::string> quote_includes,
    std::vector<std::string> includes, std::vector<std::string> system_includes) {
    xray::hexagon::model::TranslationUnitObservation obs;
    obs.reference.source_path = source_path;
    obs.reference.unique_key = source_path + "|context";
    obs.reference.source_path_key = source_path;
    obs.resolved_source_path = source_path;
    obs.quote_include_paths = std::move(quote_includes);
    obs.include_paths = std::move(includes);
    obs.system_include_paths = std::move(system_includes);
    return obs;
}

}  // namespace

TEST_CASE("classify_include_origin returns project for headers under source_root") {
    const auto obs = make_observation_with_paths("/project/src/main.cpp", {}, {}, {});
    const auto origin = xray::hexagon::services::classify_include_origin(
        "/project/include/foo.h", {obs}, std::filesystem::path{"/project"});
    CHECK(origin == xray::hexagon::model::IncludeOrigin::project);
}

TEST_CASE("classify_include_origin returns external for headers under system_include_paths") {
    const auto obs =
        make_observation_with_paths("/project/src/main.cpp", {}, {}, {"/usr/include"});
    const auto origin = xray::hexagon::services::classify_include_origin(
        "/usr/include/iostream", {obs}, std::filesystem::path{"/project"});
    CHECK(origin == xray::hexagon::model::IncludeOrigin::external);
}

TEST_CASE("classify_include_origin returns project for quote_include_paths outside source_root") {
    const auto obs = make_observation_with_paths("/project/src/main.cpp", {"/sibling/quoted"}, {},
                                                 {});
    const auto origin = xray::hexagon::services::classify_include_origin(
        "/sibling/quoted/foo.h", {obs}, std::filesystem::path{"/project"});
    CHECK(origin == xray::hexagon::model::IncludeOrigin::project);
}

TEST_CASE("classify_include_origin returns project for include_paths outside source_root") {
    const auto obs =
        make_observation_with_paths("/project/src/main.cpp", {}, {"/sibling/include"}, {});
    const auto origin = xray::hexagon::services::classify_include_origin(
        "/sibling/include/foo.h", {obs}, std::filesystem::path{"/project"});
    CHECK(origin == xray::hexagon::model::IncludeOrigin::project);
}

TEST_CASE("classify_include_origin returns unknown for headers not in any path") {
    const auto obs = make_observation_with_paths("/project/src/main.cpp", {}, {}, {});
    const auto origin = xray::hexagon::services::classify_include_origin(
        "/nowhere/foo.h", {obs}, std::filesystem::path{"/project"});
    CHECK(origin == xray::hexagon::model::IncludeOrigin::unknown);
}

TEST_CASE("classify_include_origin tolerates empty source_root and falls through to other tests") {
    const auto obs =
        make_observation_with_paths("/project/src/main.cpp", {}, {"/repo/include"}, {});
    const auto origin = xray::hexagon::services::classify_include_origin(
        "/repo/include/foo.h", {obs}, std::filesystem::path{});
    CHECK(origin == xray::hexagon::model::IncludeOrigin::project);
}

TEST_CASE("classify_include_origin uses segment matching, not string prefix") {
    const auto obs = make_observation_with_paths("/repo2/src/main.cpp", {}, {}, {});
    const auto origin = xray::hexagon::services::classify_include_origin(
        "/repo2/include/foo.h", {obs}, std::filesystem::path{"/repo"});
    CHECK(origin == xray::hexagon::model::IncludeOrigin::unknown);
}

TEST_CASE("classify_include_origin: system wins over quote include path") {
    const auto obs = make_observation_with_paths("/project/src/main.cpp", {"/sysroot"}, {},
                                                 {"/sysroot"});
    const auto origin = xray::hexagon::services::classify_include_origin(
        "/sysroot/foo.h", {obs}, std::filesystem::path{"/project"});
    CHECK(origin == xray::hexagon::model::IncludeOrigin::external);
}

TEST_CASE("classify_include_origin: source_root wins over system_include_paths") {
    const auto obs =
        make_observation_with_paths("/project/src/main.cpp", {}, {}, {"/project/include"});
    const auto origin = xray::hexagon::services::classify_include_origin(
        "/project/include/foo.h", {obs}, std::filesystem::path{"/project"});
    CHECK(origin == xray::hexagon::model::IncludeOrigin::project);
}

TEST_CASE("build_include_hotspots aggregates depth_kind direct/indirect/mixed correctly") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/a.cpp", "/project/build/a",
                                         {"clang++", "-c", "/project/src/a.cpp"}),
            CompileEntry::from_arguments("/project/src/b.cpp", "/project/build/b",
                                         {"clang++", "-c", "/project/src/b.cpp"}),
        },
        std::filesystem::path{"/tmp"});

    IncludeResolutionResult resolution;
    resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/project/include/only-direct.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/only-indirect.h", IncludeDepthKind::indirect},
             IncludeEntry{"/project/include/mixed.h", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/project/include/only-direct.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/only-indirect.h", IncludeDepthKind::indirect},
             IncludeEntry{"/project/include/mixed.h", IncludeDepthKind::indirect}},
            {},
        },
    };

    const auto build = xray::hexagon::services::build_include_hotspots(
        observations, resolution, std::filesystem::path{"/tmp"}, std::filesystem::path{},
        xray::hexagon::services::IncludeHotspotFilters{});

    REQUIRE(build.hotspots.size() == 3);
    const auto find_kind = [&](std::string_view name) {
        for (const auto& h : build.hotspots) {
            if (h.header_path.find(name) != std::string::npos) return h.depth_kind;
        }
        return xray::hexagon::model::IncludeDepthKind::direct;
    };
    CHECK(find_kind("only-direct") == xray::hexagon::model::IncludeDepthKind::direct);
    CHECK(find_kind("only-indirect") == xray::hexagon::model::IncludeDepthKind::indirect);
    CHECK(find_kind("mixed") == xray::hexagon::model::IncludeDepthKind::mixed);
}

TEST_CASE("build_include_hotspots scope=project filters out external+unknown and counts unknown exclusions") {
    auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/a.cpp", "/project/build/a",
                                         {"clang++", "-c", "/project/src/a.cpp"}),
            CompileEntry::from_arguments("/project/src/b.cpp", "/project/build/b",
                                         {"clang++", "-c", "/project/src/b.cpp"}),
        },
        std::filesystem::path{"/tmp"});
    observations[0].system_include_paths.push_back("/usr/include");
    observations[1].system_include_paths.push_back("/usr/include");

    IncludeResolutionResult resolution;
    resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/project/include/proj.h", IncludeDepthKind::direct},
             IncludeEntry{"/usr/include/iostream", IncludeDepthKind::direct},
             IncludeEntry{"/nowhere/orphan.h", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/project/include/proj.h", IncludeDepthKind::direct},
             IncludeEntry{"/usr/include/iostream", IncludeDepthKind::direct},
             IncludeEntry{"/nowhere/orphan.h", IncludeDepthKind::direct}},
            {},
        },
    };

    const auto build = xray::hexagon::services::build_include_hotspots(
        observations, resolution, std::filesystem::path{"/tmp"},
        std::filesystem::path{"/project"},
        xray::hexagon::services::IncludeHotspotFilters{
            xray::hexagon::model::IncludeScope::project,
            xray::hexagon::model::IncludeDepthFilter::all});

    CHECK(build.total_count == 3);
    REQUIRE(build.hotspots.size() == 1);
    CHECK(build.hotspots[0].origin == xray::hexagon::model::IncludeOrigin::project);
    CHECK(build.excluded_unknown_count == 1);
    CHECK(build.excluded_mixed_count == 0);
}

TEST_CASE("build_include_hotspots depth=direct filters out indirect+mixed and counts mixed exclusions") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/a.cpp", "/project/build/a",
                                         {"clang++", "-c", "/project/src/a.cpp"}),
            CompileEntry::from_arguments("/project/src/b.cpp", "/project/build/b",
                                         {"clang++", "-c", "/project/src/b.cpp"}),
        },
        std::filesystem::path{"/tmp"});

    IncludeResolutionResult resolution;
    resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/project/include/d.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/i.h", IncludeDepthKind::indirect},
             IncludeEntry{"/project/include/m.h", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/project/include/d.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/i.h", IncludeDepthKind::indirect},
             IncludeEntry{"/project/include/m.h", IncludeDepthKind::indirect}},
            {},
        },
    };

    const auto build = xray::hexagon::services::build_include_hotspots(
        observations, resolution, std::filesystem::path{"/tmp"}, std::filesystem::path{},
        xray::hexagon::services::IncludeHotspotFilters{
            xray::hexagon::model::IncludeScope::all,
            xray::hexagon::model::IncludeDepthFilter::direct});

    CHECK(build.total_count == 3);
    REQUIRE(build.hotspots.size() == 1);
    CHECK(build.hotspots[0].header_path == "/project/include/d.h");
    CHECK(build.excluded_mixed_count == 1);
    CHECK(build.excluded_unknown_count == 0);
}

TEST_CASE("build_include_hotspots scope=external returns only external hotspots") {
    auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/a.cpp", "/project/build/a",
                                         {"clang++", "-c", "/project/src/a.cpp"}),
            CompileEntry::from_arguments("/project/src/b.cpp", "/project/build/b",
                                         {"clang++", "-c", "/project/src/b.cpp"}),
        },
        std::filesystem::path{"/tmp"});
    observations[0].system_include_paths.push_back("/usr/include");
    observations[1].system_include_paths.push_back("/usr/include");

    IncludeResolutionResult resolution;
    resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/project/include/proj.h", IncludeDepthKind::direct},
             IncludeEntry{"/usr/include/iostream", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/project/include/proj.h", IncludeDepthKind::direct},
             IncludeEntry{"/usr/include/iostream", IncludeDepthKind::direct}},
            {},
        },
    };

    const auto build = xray::hexagon::services::build_include_hotspots(
        observations, resolution, std::filesystem::path{"/tmp"},
        std::filesystem::path{"/project"},
        xray::hexagon::services::IncludeHotspotFilters{
            xray::hexagon::model::IncludeScope::external,
            xray::hexagon::model::IncludeDepthFilter::all});

    REQUIRE(build.hotspots.size() == 1);
    CHECK(build.hotspots[0].origin == xray::hexagon::model::IncludeOrigin::external);
}

TEST_CASE("build_include_hotspots scope=unknown returns only unknown-origin hotspots") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/a.cpp", "/project/build/a",
                                         {"clang++", "-c", "/project/src/a.cpp"}),
            CompileEntry::from_arguments("/project/src/b.cpp", "/project/build/b",
                                         {"clang++", "-c", "/project/src/b.cpp"}),
        },
        std::filesystem::path{"/tmp"});

    IncludeResolutionResult resolution;
    resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/project/include/proj.h", IncludeDepthKind::direct},
             IncludeEntry{"/nowhere/orphan.h", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/project/include/proj.h", IncludeDepthKind::direct},
             IncludeEntry{"/nowhere/orphan.h", IncludeDepthKind::direct}},
            {},
        },
    };

    const auto build = xray::hexagon::services::build_include_hotspots(
        observations, resolution, std::filesystem::path{"/tmp"},
        std::filesystem::path{"/project"},
        xray::hexagon::services::IncludeHotspotFilters{
            xray::hexagon::model::IncludeScope::unknown,
            xray::hexagon::model::IncludeDepthFilter::all});

    REQUIRE(build.hotspots.size() == 1);
    CHECK(build.hotspots[0].origin == xray::hexagon::model::IncludeOrigin::unknown);
}

TEST_CASE("build_include_hotspots depth=indirect returns only indirect hotspots") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/a.cpp", "/project/build/a",
                                         {"clang++", "-c", "/project/src/a.cpp"}),
            CompileEntry::from_arguments("/project/src/b.cpp", "/project/build/b",
                                         {"clang++", "-c", "/project/src/b.cpp"}),
        },
        std::filesystem::path{"/tmp"});

    IncludeResolutionResult resolution;
    resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/project/include/d.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/i.h", IncludeDepthKind::indirect}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/project/include/d.h", IncludeDepthKind::direct},
             IncludeEntry{"/project/include/i.h", IncludeDepthKind::indirect}},
            {},
        },
    };

    const auto build = xray::hexagon::services::build_include_hotspots(
        observations, resolution, std::filesystem::path{"/tmp"}, std::filesystem::path{},
        xray::hexagon::services::IncludeHotspotFilters{
            xray::hexagon::model::IncludeScope::all,
            xray::hexagon::model::IncludeDepthFilter::indirect});

    REQUIRE(build.hotspots.size() == 1);
    CHECK(build.hotspots[0].header_path == "/project/include/i.h");
    CHECK(build.hotspots[0].depth_kind == xray::hexagon::model::IncludeDepthKind::indirect);
}

TEST_CASE("build_include_hotspots scope and depth filters compose as intersection without double-counting unknown+mixed") {
    auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/a.cpp", "/project/build/a",
                                         {"clang++", "-c", "/project/src/a.cpp"}),
            CompileEntry::from_arguments("/project/src/b.cpp", "/project/build/b",
                                         {"clang++", "-c", "/project/src/b.cpp"}),
        },
        std::filesystem::path{"/tmp"});

    IncludeResolutionResult resolution;
    resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/nowhere/orphan.h", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/nowhere/orphan.h", IncludeDepthKind::indirect}},
            {},
        },
    };

    const auto build = xray::hexagon::services::build_include_hotspots(
        observations, resolution, std::filesystem::path{"/tmp"}, std::filesystem::path{"/project"},
        xray::hexagon::services::IncludeHotspotFilters{
            xray::hexagon::model::IncludeScope::project,
            xray::hexagon::model::IncludeDepthFilter::direct});

    CHECK(build.total_count == 1);
    CHECK(build.hotspots.empty());
    CHECK(build.excluded_unknown_count == 1);
    CHECK(build.excluded_mixed_count == 0);
}

TEST_CASE("build_include_hotspots: project+mixed hotspot rejected by scope=unknown does not count under any exclusion bucket") {
    const auto observations = xray::hexagon::services::build_translation_unit_observations(
        {
            CompileEntry::from_arguments("/project/src/a.cpp", "/project/build/a",
                                         {"clang++", "-c", "/project/src/a.cpp"}),
            CompileEntry::from_arguments("/project/src/b.cpp", "/project/build/b",
                                         {"clang++", "-c", "/project/src/b.cpp"}),
        },
        std::filesystem::path{"/tmp"});

    IncludeResolutionResult resolution;
    resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {IncludeEntry{"/project/include/m.h", IncludeDepthKind::direct}},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {IncludeEntry{"/project/include/m.h", IncludeDepthKind::indirect}},
            {},
        },
    };

    const auto build = xray::hexagon::services::build_include_hotspots(
        observations, resolution, std::filesystem::path{"/tmp"}, std::filesystem::path{"/project"},
        xray::hexagon::services::IncludeHotspotFilters{
            xray::hexagon::model::IncludeScope::unknown,
            xray::hexagon::model::IncludeDepthFilter::direct});

    CHECK(build.total_count == 1);
    CHECK(build.hotspots.empty());
    CHECK(build.excluded_unknown_count == 0);
    CHECK(build.excluded_mixed_count == 0);
}

TEST_CASE("analysis support normalizes relative paths via absolute resolution") {
    const auto result = xray::hexagon::services::normalize_path("relative/path.cpp");

    CHECK_FALSE(result.empty());
    // Relative path must be resolved to an absolute path containing the input suffix
    CHECK(result.find("relative/path.cpp") != std::string::npos);
    CHECK(std::filesystem::path{result}.is_absolute());
}

TEST_CASE("compile commands base directory uses explicit fallback when path has no parent") {
    const auto base = xray::hexagon::services::compile_commands_base_directory(
        "compile_commands.json", std::filesystem::path{"/explicit/base"});
    CHECK(base == std::filesystem::path{"/explicit/base"});
}

TEST_CASE("compile commands base directory keeps parent when path has one") {
    const auto base = xray::hexagon::services::compile_commands_base_directory(
        "/tmp/build/compile_commands.json", std::filesystem::path{"/explicit/base"});
    CHECK(base == std::filesystem::path{"/tmp/build"});
}

TEST_CASE("compile commands base directory resolves relative path against fallback base") {
    const auto base = xray::hexagon::services::compile_commands_base_directory(
        "build/compile_commands.json", std::filesystem::path{"/repo"});
    CHECK(base == std::filesystem::path{"/repo/build"});
}

TEST_CASE("source root from build model returns nullopt for empty source root") {
    xray::hexagon::model::BuildModelResult model;
    model.source_root = "";
    CHECK_FALSE(xray::hexagon::services::source_root_from_build_model(model).has_value());
}

TEST_CASE("source root from build model returns path for non-empty source root") {
    xray::hexagon::model::BuildModelResult model;
    model.source_root = "/project";
    const auto root = xray::hexagon::services::source_root_from_build_model(model);
    REQUIRE(root.has_value());
    CHECK(*root == std::filesystem::path{"/project"});
}

TEST_CASE("to_report_display_path returns nullopt without a display path") {
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    CHECK_FALSE(
        to_report_display_path({std::nullopt, ReportPathDisplayKind::input_argument, false},
                               std::filesystem::path{"/repo"})
            .has_value());
}

TEST_CASE("to_report_display_path normalizes input arguments lexically") {
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    const auto absolute = to_report_display_path(
        {std::filesystem::path{"/repo/build/compile_commands.json"},
         ReportPathDisplayKind::input_argument, false},
        std::filesystem::path{"/repo"});
    REQUIRE(absolute.has_value());
    CHECK(*absolute == "/repo/build/compile_commands.json");

    const auto relative = to_report_display_path(
        {std::filesystem::path{"./build/../out/compile_commands.json"},
         ReportPathDisplayKind::input_argument, true},
        std::filesystem::path{"/repo"});
    REQUIRE(relative.has_value());
    CHECK(*relative == "out/compile_commands.json");
}

TEST_CASE("to_report_display_path keeps absolute resolved adapter path when source was absolute") {
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    const auto display = to_report_display_path(
        {std::filesystem::path{"/repo/build/.cmake/api/v1/reply"},
         ReportPathDisplayKind::resolved_adapter_path, false},
        std::filesystem::path{"/repo"});
    REQUIRE(display.has_value());
    CHECK(*display == "/repo/build/.cmake/api/v1/reply");
}

TEST_CASE("to_report_display_path relativizes resolved adapter path under base when source was relative") {
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    const auto display = to_report_display_path(
        {abs_path("/repo/build/.cmake/api/v1/reply"),
         ReportPathDisplayKind::resolved_adapter_path, true},
        abs_path("/repo"));
    REQUIRE(display.has_value());
    CHECK(*display == "build/.cmake/api/v1/reply");
}

TEST_CASE("to_report_display_path keeps absolute when adapter path is not under base") {
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    const auto display = to_report_display_path(
        {std::filesystem::path{"/repo2/build/.cmake/api/v1/reply"},
         ReportPathDisplayKind::resolved_adapter_path, true},
        std::filesystem::path{"/repo"});
    REQUIRE(display.has_value());
    CHECK(*display == "/repo2/build/.cmake/api/v1/reply");
}

TEST_CASE("to_report_display_path returns dot when adapter path equals base") {
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    const auto display = to_report_display_path(
        {abs_path("/repo"),
         ReportPathDisplayKind::resolved_adapter_path, true},
        abs_path("/repo"));
    REQUIRE(display.has_value());
    CHECK(*display == ".");
}

TEST_CASE("to_report_display_path keeps relative adapter path lexically normalized") {
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    const auto display = to_report_display_path(
        {std::filesystem::path{"build/./reply"},
         ReportPathDisplayKind::resolved_adapter_path, true},
        std::filesystem::path{"/repo"});
    REQUIRE(display.has_value());
    CHECK(*display == "build/reply");
}

TEST_CASE("to_report_display_path with case_sensitive policy keeps mixed case under different paths") {
    using xray::hexagon::services::ReportPathCasePolicy;
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    // POSIX semantics: /Repo and /repo are different paths.
    const auto display = to_report_display_path(
        {std::filesystem::path{"/Repo/build/.cmake/api/v1/reply"},
         ReportPathDisplayKind::resolved_adapter_path, true},
        std::filesystem::path{"/repo"}, ReportPathCasePolicy::case_sensitive);
    REQUIRE(display.has_value());
    CHECK(*display == "/Repo/build/.cmake/api/v1/reply");
}

TEST_CASE("to_report_display_path with case_insensitive policy folds drive and segment case") {
    using xray::hexagon::services::ReportPathCasePolicy;
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    // Windows semantics: /Repo and /repo are the same path; display preserves
    // the original case of the resolved adapter path.
    const auto display = to_report_display_path(
        {abs_path("/Repo/Build/.cmake/api/v1/reply"),
         ReportPathDisplayKind::resolved_adapter_path, true},
        abs_path("/repo"), ReportPathCasePolicy::case_insensitive);
    REQUIRE(display.has_value());
    CHECK(*display == "Build/.cmake/api/v1/reply");
}

TEST_CASE("to_report_display_path with case_insensitive policy treats equal-with-different-case as base") {
    using xray::hexagon::services::ReportPathCasePolicy;
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    const auto display = to_report_display_path(
        {abs_path("/REPO"),
         ReportPathDisplayKind::resolved_adapter_path, true},
        abs_path("/repo"), ReportPathCasePolicy::case_insensitive);
    REQUIRE(display.has_value());
    CHECK(*display == ".");
}

TEST_CASE("to_report_display_path with case_insensitive policy still rejects unrelated bases") {
    using xray::hexagon::services::ReportPathCasePolicy;
    using xray::hexagon::services::ReportPathDisplayKind;
    using xray::hexagon::services::to_report_display_path;
    const auto display = to_report_display_path(
        {std::filesystem::path{"/repo2/build"},
         ReportPathDisplayKind::resolved_adapter_path, true},
        std::filesystem::path{"/repo"}, ReportPathCasePolicy::case_insensitive);
    REQUIRE(display.has_value());
    CHECK(*display == "/repo2/build");
}

TEST_CASE("report format version is three") {
    // AP M6-1.2 bumped the contract from 1 to 2 with the target-graph and
    // target-hubs additions to the analyze JSON (impact JSON adds the graph
    // but not the hubs). AP M6-1.3 A.4 lifts it to 3 so the prioritised
    // impact view reaches all five report formats. All adapters must
    // reuse this same constant.
    CHECK(xray::hexagon::model::kReportFormatVersion == 3);
}

TEST_CASE("report-json schema format_version const matches kReportFormatVersion") {
    // AP 1.2 Tranche A gate: spec/report-json.schema.json must declare a
    // const value for FormatVersion that matches the C++ constant. Mismatches
    // are a hard fail because the schema, the adapter, and any downstream
    // tooling have to agree on a single version source.
    // Schema path comes from a compile definition so the test does not depend
    // on the current working directory and stays correct if WORKING_DIRECTORY
    // is ever dropped from xray_tests.
    const auto schema_path = std::filesystem::path{XRAY_REPORT_JSON_SCHEMA_PATH};
    std::ifstream stream{schema_path};
    REQUIRE(stream.is_open());
    nlohmann::json schema;
    stream >> schema;
    REQUIRE(schema.contains("$defs"));
    REQUIRE(schema["$defs"].contains("FormatVersion"));
    const auto& format_version_def = schema["$defs"]["FormatVersion"];
    REQUIRE(format_version_def.contains("const"));
    CHECK(format_version_def["const"].get<int>() ==
          xray::hexagon::model::kReportFormatVersion);
}
