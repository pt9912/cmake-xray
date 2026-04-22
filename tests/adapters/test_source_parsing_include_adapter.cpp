#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "adapters/input/source_parsing_include_adapter.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/services/analysis_support.h"

namespace {

using xray::adapters::input::SourceParsingIncludeAdapter;
using xray::hexagon::model::CompileEntry;

class TempDir {
public:
    explicit TempDir(const std::string& name)
        : path_(std::filesystem::temp_directory_path() / name) {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TempDir() { std::filesystem::remove_all(path_); }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void write_file(const std::filesystem::path& path, std::string_view contents) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    file << contents;
}

std::vector<xray::hexagon::model::TranslationUnitObservation> build_single_observation(
    const std::filesystem::path& fixture_root, CompileEntry entry) {
    const auto compile_commands_path = fixture_root / "compile_commands.json";
    return xray::hexagon::services::build_translation_unit_observations({std::move(entry)},
                                                                        compile_commands_path.string());
}

}  // namespace

TEST_CASE("source parsing include adapter resolves direct and transitive includes") {
    TempDir temp_dir{"cmake-xray-source-parsing-direct"};
    write_file(temp_dir.path() / "src/main.cpp", "#include \"common/config.h\"\n");
    write_file(temp_dir.path() / "include/common/config.h", "#include \"common/shared.h\"\n");
    write_file(temp_dir.path() / "include/common/shared.h", "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    REQUIRE(result.translation_units[0].headers.size() == 2);
    CHECK(result.translation_units[0].headers[0] ==
          xray::hexagon::services::normalize_path(temp_dir.path() / "include/common/config.h"));
    CHECK(result.translation_units[0].headers[1] ==
          xray::hexagon::services::normalize_path(temp_dir.path() / "include/common/shared.h"));
    CHECK(result.translation_units[0].diagnostics.empty());
}

TEST_CASE("source parsing include adapter applies quoted and angled search order") {
    TempDir temp_dir{"cmake-xray-source-parsing-search-order"};
    write_file(temp_dir.path() / "src/main.cpp",
               "#include \"same/header.h\"\n#include <same/system.h>\n");
    write_file(temp_dir.path() / "src/same/header.h", "#pragma once\n");
    write_file(temp_dir.path() / "include/same/header.h", "#pragma once\n");
    write_file(temp_dir.path() / "include/same/system.h", "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    REQUIRE(result.translation_units[0].headers.size() == 2);
    CHECK(result.translation_units[0].headers[0] ==
          xray::hexagon::services::normalize_path(temp_dir.path() / "include/same/system.h"));
    CHECK(result.translation_units[0].headers[1] ==
          xray::hexagon::services::normalize_path(temp_dir.path() / "src/same/header.h"));
}

TEST_CASE("source parsing include adapter handles cycles and unresolved includes") {
    TempDir temp_dir{"cmake-xray-source-parsing-cycle"};
    write_file(temp_dir.path() / "src/main.cpp", "#include \"cycle/a.h\"\n");
    write_file(temp_dir.path() / "include/cycle/a.h", "#include \"cycle/b.h\"\n");
    write_file(temp_dir.path() / "include/cycle/b.h",
               "#include \"cycle/a.h\"\n#include \"cycle/missing.h\"\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    CHECK(result.translation_units[0].headers.size() == 2);
    REQUIRE_FALSE(result.translation_units[0].diagnostics.empty());
    CHECK(result.translation_units[0].diagnostics[0].message.find("could not resolve include") !=
          std::string::npos);
}
