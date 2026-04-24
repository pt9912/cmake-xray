#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/services/analysis_support.h"

namespace {

using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::ResolvedTranslationUnitIncludes;

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
        std::string_view{"/tmp/compile_commands.json"});

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
        std::string_view{"/tmp/compile_commands.json"});

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
        std::string_view{"/tmp/compile_commands.json"});

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
        std::string_view{"/tmp/compile_commands.json"});

    IncludeResolutionResult include_resolution;
    include_resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {"/project/include/b.h", "/project/include/a.h"},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {"/project/include/b.h", "/project/include/a.h"},
            {},
        },
    };

    const auto hotspots = xray::hexagon::services::build_include_hotspots(
        observations, include_resolution, std::string_view{"/tmp/compile_commands.json"});

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
        std::string_view{"/tmp/compile_commands.json"});

    IncludeResolutionResult include_resolution;
    include_resolution.translation_units = {
        ResolvedTranslationUnitIncludes{
            observations[0].reference.unique_key,
            {"/project/include/smaller.h", "/project/include/larger.h"},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[1].reference.unique_key,
            {"/project/include/smaller.h", "/project/include/larger.h"},
            {},
        },
        ResolvedTranslationUnitIncludes{
            observations[2].reference.unique_key,
            {"/project/include/larger.h"},
            {},
        },
    };

    const auto hotspots = xray::hexagon::services::build_include_hotspots(
        observations, include_resolution, std::string_view{"/tmp/compile_commands.json"});

    REQUIRE(hotspots.size() == 2);
    CHECK(hotspots[0].header_path == "/project/include/larger.h");
    CHECK(hotspots[1].header_path == "/project/include/smaller.h");
}

TEST_CASE("analysis support normalizes relative paths via absolute resolution") {
    const auto result = xray::hexagon::services::normalize_path("relative/path.cpp");

    CHECK_FALSE(result.empty());
    // Relative path must be resolved to an absolute path containing the input suffix
    CHECK(result.find("relative/path.cpp") != std::string::npos);
    CHECK(std::filesystem::path{result}.is_absolute());
}
