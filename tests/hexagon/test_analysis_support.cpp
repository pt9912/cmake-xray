#include <doctest/doctest.h>

#include <string>
#include <vector>

#include <filesystem>
#include <optional>

#include "hexagon/model/build_model_result.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/model/report_format_version.h"
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
        observations, include_resolution, std::filesystem::path{"/tmp"});

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
        observations, include_resolution, std::filesystem::path{"/tmp"});

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
        {std::filesystem::path{"/repo/build/.cmake/api/v1/reply"},
         ReportPathDisplayKind::resolved_adapter_path, true},
        std::filesystem::path{"/repo"});
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
        {std::filesystem::path{"/repo"},
         ReportPathDisplayKind::resolved_adapter_path, true},
        std::filesystem::path{"/repo"});
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

TEST_CASE("report format version is one") {
    // AP 1.2 JSON adapter must reuse this same constant.
    CHECK(xray::hexagon::model::kReportFormatVersion == 1);
}
