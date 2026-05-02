#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "adapters/input/source_parsing_include_adapter.h"
#include "hexagon/model/compile_entry.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/services/analysis_support.h"

namespace {

using xray::adapters::input::SourceParsingIncludeAdapter;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::IncludeDepthKind;
using xray::hexagon::model::IncludeEntry;

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
    return xray::hexagon::services::build_translation_unit_observations(
        {std::move(entry)}, fixture_root);
}

bool contains_header(const std::vector<IncludeEntry>& headers, const std::string& path) {
    return std::any_of(headers.begin(), headers.end(),
                       [&](const IncludeEntry& entry) { return entry.header_path == path; });
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
    CHECK(result.translation_units[0].headers[0].header_path ==
          xray::hexagon::services::normalize_path(temp_dir.path() / "include/common/config.h"));
    CHECK(result.translation_units[0].headers[0].depth_kind == IncludeDepthKind::direct);
    CHECK(result.translation_units[0].headers[1].header_path ==
          xray::hexagon::services::normalize_path(temp_dir.path() / "include/common/shared.h"));
    CHECK(result.translation_units[0].headers[1].depth_kind == IncludeDepthKind::indirect);
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
    CHECK(result.translation_units[0].headers[0].header_path ==
          xray::hexagon::services::normalize_path(temp_dir.path() / "include/same/system.h"));
    CHECK(result.translation_units[0].headers[1].header_path ==
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
    // BFS with depth-kind awareness emits a.h twice (direct from main, indirect via b.h cycle)
    // plus b.h once, so three IncludeEntry entries are expected per AP 1.4 design.
    CHECK(result.translation_units[0].headers.size() == 3);
    REQUIRE_FALSE(result.translation_units[0].diagnostics.empty());
    CHECK(result.translation_units[0].diagnostics[0].message.find("could not resolve include") !=
          std::string::npos);
}

TEST_CASE("source parsing include adapter resolves relative -I, -iquote and -isystem paths") {
    TempDir temp_dir{"cmake-xray-source-parsing-relative-include-paths"};
    write_file(temp_dir.path() / "src/main.cpp",
               "#include \"only_quote.h\"\n#include <only_public.h>\n#include <only_system.h>\n");
    write_file(temp_dir.path() / "quoted/only_quote.h", "#pragma once\n");
    write_file(temp_dir.path() / "include/only_public.h", "#pragma once\n");
    write_file(temp_dir.path() / "system/only_system.h", "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments(
            "../src/main.cpp", "build",
            {"clang++", "-iquote../quoted", "-I../include", "-isystem", "../system", "-c",
             "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);
    const auto quoted_header =
        xray::hexagon::services::normalize_path(temp_dir.path() / "quoted/only_quote.h");
    const auto include_header =
        xray::hexagon::services::normalize_path(temp_dir.path() / "include/only_public.h");
    const auto system_header =
        xray::hexagon::services::normalize_path(temp_dir.path() / "system/only_system.h");

    REQUIRE(result.translation_units.size() == 1);
    REQUIRE(result.translation_units[0].headers.size() == 3);
    CHECK(contains_header(result.translation_units[0].headers, quoted_header));
    CHECK(contains_header(result.translation_units[0].headers, include_header));
    CHECK(contains_header(result.translation_units[0].headers, system_header));
    CHECK(result.translation_units[0].diagnostics.empty());
}

TEST_CASE("source parsing include adapter respects -iquote before -I and -I before -isystem") {
    TempDir temp_dir{"cmake-xray-source-parsing-search-precedence"};
    write_file(temp_dir.path() / "src/main.cpp",
               "#include \"same/header.h\"\n#include <same/system.h>\n");
    write_file(temp_dir.path() / "quoted/same/header.h", "#pragma once\n");
    write_file(temp_dir.path() / "include/same/header.h", "#pragma once\n");
    write_file(temp_dir.path() / "system/same/header.h", "#pragma once\n");
    write_file(temp_dir.path() / "include/same/system.h", "#pragma once\n");
    write_file(temp_dir.path() / "system/same/system.h", "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments(
            "../src/main.cpp", "build",
            {"clang++", "-iquote../quoted", "-I../include", "-isystem", "../system", "-c",
             "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);
    const auto quoted_header =
        xray::hexagon::services::normalize_path(temp_dir.path() / "quoted/same/header.h");
    const auto include_header =
        xray::hexagon::services::normalize_path(temp_dir.path() / "include/same/system.h");

    REQUIRE(result.translation_units.size() == 1);
    REQUIRE(result.translation_units[0].headers.size() == 2);
    CHECK(contains_header(result.translation_units[0].headers, quoted_header));
    CHECK(contains_header(result.translation_units[0].headers, include_header));
    CHECK(result.translation_units[0].diagnostics.empty());
}

TEST_CASE("source parsing include adapter revisits fully visited headers without diagnostics") {
    TempDir temp_dir{"cmake-xray-source-parsing-fully-visited"};
    write_file(temp_dir.path() / "src/main.cpp",
               "#include \"branch/a.h\"\n#include \"branch/b.h\"\n");
    write_file(temp_dir.path() / "include/branch/a.h", "#include \"branch/shared.h\"\n");
    write_file(temp_dir.path() / "include/branch/b.h", "#include \"branch/shared.h\"\n");
    write_file(temp_dir.path() / "include/branch/shared.h", "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    REQUIRE(result.translation_units[0].headers.size() == 3);
    CHECK(result.translation_units[0].diagnostics.empty());
}

TEST_CASE("source parsing include adapter marks direct depth_kind for includes from the TU") {
    TempDir temp_dir{"cmake-xray-source-parsing-depth-kind-direct"};
    write_file(temp_dir.path() / "src/main.cpp", "#include \"only/direct.h\"\n");
    write_file(temp_dir.path() / "include/only/direct.h", "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    REQUIRE(result.translation_units[0].headers.size() == 1);
    CHECK(result.translation_units[0].headers[0].depth_kind == IncludeDepthKind::direct);
}

TEST_CASE("source parsing include adapter marks indirect depth_kind for transitively included headers") {
    TempDir temp_dir{"cmake-xray-source-parsing-depth-kind-indirect"};
    write_file(temp_dir.path() / "src/main.cpp", "#include \"chain/a.h\"\n");
    write_file(temp_dir.path() / "include/chain/a.h", "#include \"chain/b.h\"\n");
    write_file(temp_dir.path() / "include/chain/b.h", "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    REQUIRE(result.translation_units[0].headers.size() == 2);
    const auto a_path =
        xray::hexagon::services::normalize_path(temp_dir.path() / "include/chain/a.h");
    const auto b_path =
        xray::hexagon::services::normalize_path(temp_dir.path() / "include/chain/b.h");
    const auto a_it = std::find_if(
        result.translation_units[0].headers.begin(), result.translation_units[0].headers.end(),
        [&](const IncludeEntry& entry) { return entry.header_path == a_path; });
    const auto b_it = std::find_if(
        result.translation_units[0].headers.begin(), result.translation_units[0].headers.end(),
        [&](const IncludeEntry& entry) { return entry.header_path == b_path; });
    REQUIRE(a_it != result.translation_units[0].headers.end());
    REQUIRE(b_it != result.translation_units[0].headers.end());
    CHECK(a_it->depth_kind == IncludeDepthKind::direct);
    CHECK(b_it->depth_kind == IncludeDepthKind::indirect);
}

TEST_CASE("source parsing include adapter emits both depth_kinds for header reached directly and transitively") {
    TempDir temp_dir{"cmake-xray-source-parsing-depth-kind-mixed"};
    write_file(temp_dir.path() / "src/main.cpp",
               "#include \"mix/wrapper.h\"\n#include \"mix/shared.h\"\n");
    write_file(temp_dir.path() / "include/mix/wrapper.h", "#include \"mix/shared.h\"\n");
    write_file(temp_dir.path() / "include/mix/shared.h", "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    const auto shared_path =
        xray::hexagon::services::normalize_path(temp_dir.path() / "include/mix/shared.h");
    const auto direct_count = std::count_if(
        result.translation_units[0].headers.begin(), result.translation_units[0].headers.end(),
        [&](const IncludeEntry& entry) {
            return entry.header_path == shared_path && entry.depth_kind == IncludeDepthKind::direct;
        });
    const auto indirect_count = std::count_if(
        result.translation_units[0].headers.begin(), result.translation_units[0].headers.end(),
        [&](const IncludeEntry& entry) {
            return entry.header_path == shared_path &&
                   entry.depth_kind == IncludeDepthKind::indirect;
        });
    CHECK(direct_count == 1);
    CHECK(indirect_count == 1);
}

TEST_CASE("source parsing include adapter terminates on cyclic header includes without cycle diagnostics") {
    TempDir temp_dir{"cmake-xray-source-parsing-cycle-no-diag"};
    write_file(temp_dir.path() / "src/main.cpp", "#include \"loop/a.h\"\n");
    write_file(temp_dir.path() / "include/loop/a.h", "#include \"loop/b.h\"\n");
    write_file(temp_dir.path() / "include/loop/b.h", "#include \"loop/a.h\"\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    CHECK(result.translation_units[0].diagnostics.empty());
    // a.h appears twice (direct from main, indirect via b.h's cycle back) plus b.h once.
    REQUIRE(result.translation_units[0].headers.size() == 3);
}

TEST_CASE("source parsing include adapter terminates on self-including header") {
    TempDir temp_dir{"cmake-xray-source-parsing-self-include"};
    write_file(temp_dir.path() / "src/main.cpp", "#include \"self/me.h\"\n");
    write_file(temp_dir.path() / "include/self/me.h", "#include \"self/me.h\"\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    // me.h appears once direct (depth 1) and once indirect (depth 2 via the self-include).
    REQUIRE(result.translation_units[0].headers.size() == 2);
    CHECK(result.translation_units[0].diagnostics.empty());
}

TEST_CASE("source parsing include adapter caps include analysis at depth limit and emits depth diagnostic") {
    TempDir temp_dir{"cmake-xray-source-parsing-depth-cap"};
    write_file(temp_dir.path() / "src/main.cpp", "#include \"chain/h0.h\"\n");
    constexpr int kChainLength = 35;  // > kIncludeDepthLimit (32)
    for (int i = 0; i < kChainLength - 1; ++i) {
        write_file(temp_dir.path() / ("include/chain/h" + std::to_string(i) + ".h"),
                   "#include \"chain/h" + std::to_string(i + 1) + ".h\"\n");
    }
    write_file(temp_dir.path() / ("include/chain/h" + std::to_string(kChainLength - 1) + ".h"),
               "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    CHECK(result.translation_units[0].headers.size() == 32);
    const auto h32_path =
        xray::hexagon::services::normalize_path(temp_dir.path() / "include/chain/h32.h");
    CHECK_FALSE(contains_header(result.translation_units[0].headers, h32_path));
    CHECK(result.include_depth_limit_effective == 32);
    const auto has_depth_diag = std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(),
        [](const auto& d) { return d.message.find("depth limit reached") != std::string::npos; });
    CHECK(has_depth_diag);
}

TEST_CASE("source parsing include adapter does not emit depth diagnostic when leaf at limit has no includes") {
    TempDir temp_dir{"cmake-xray-source-parsing-depth-limit-empty-leaf"};
    write_file(temp_dir.path() / "src/main.cpp", "#include \"chain/h0.h\"\n");
    // Build a chain whose deepest member sits exactly at depth 32 and has no
    // #include directives -- the depth-limit diagnostic must stay silent
    // because nothing transitive would have been missed.
    constexpr int kChainLength = 32;
    for (int i = 0; i < kChainLength - 1; ++i) {
        write_file(temp_dir.path() / ("include/chain/h" + std::to_string(i) + ".h"),
                   "#include \"chain/h" + std::to_string(i + 1) + ".h\"\n");
    }
    write_file(temp_dir.path() / ("include/chain/h" + std::to_string(kChainLength - 1) + ".h"),
               "#pragma once\n");

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    REQUIRE(result.translation_units.size() == 1);
    CHECK(result.translation_units[0].headers.size() == 32);
    const auto has_depth_diag = std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(),
        [](const auto& d) { return d.message.find("depth limit reached") != std::string::npos; });
    CHECK_FALSE(has_depth_diag);
}

TEST_CASE("source parsing include adapter caps include analysis at node budget and emits budget diagnostic") {
    TempDir temp_dir{"cmake-xray-source-parsing-budget-cap"};
    constexpr int kHeaderCount = 10005;  // > kIncludeNodeBudget (10000)

    std::string main_contents;
    main_contents.reserve(kHeaderCount * 24);
    for (int i = 0; i < kHeaderCount; ++i) {
        main_contents += "#include \"flat/h" + std::to_string(i) + ".h\"\n";
        write_file(temp_dir.path() / ("include/flat/h" + std::to_string(i) + ".h"),
                   "#pragma once\n");
    }
    write_file(temp_dir.path() / "src/main.cpp", main_contents);

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto result = adapter.resolve_includes(observations);

    CHECK(result.include_node_budget_effective == 10000);
    CHECK(result.include_node_budget_reached);
    REQUIRE(result.translation_units.size() == 1);
    CHECK(result.translation_units[0].headers.size() == 10000);
    const auto has_budget_diag = std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(),
        [](const auto& d) { return d.message.find("budget reached") != std::string::npos; });
    CHECK(has_budget_diag);
}

TEST_CASE("source parsing include adapter is deterministic across reruns at the budget limit") {
    TempDir temp_dir{"cmake-xray-source-parsing-budget-determinism"};
    constexpr int kHeaderCount = 10005;

    std::string main_contents;
    main_contents.reserve(kHeaderCount * 24);
    for (int i = 0; i < kHeaderCount; ++i) {
        main_contents += "#include \"flat/h" + std::to_string(i) + ".h\"\n";
        write_file(temp_dir.path() / ("include/flat/h" + std::to_string(i) + ".h"),
                   "#pragma once\n");
    }
    write_file(temp_dir.path() / "src/main.cpp", main_contents);

    const auto observations = build_single_observation(
        temp_dir.path(),
        CompileEntry::from_arguments("../src/main.cpp", "build",
                                     {"clang++", "-I../include", "-c", "../src/main.cpp"}));

    const SourceParsingIncludeAdapter adapter;
    const auto first = adapter.resolve_includes(observations);
    const auto second = adapter.resolve_includes(observations);

    REQUIRE(first.translation_units.size() == 1);
    REQUIRE(second.translation_units.size() == 1);
    REQUIRE(first.translation_units[0].headers.size() ==
            second.translation_units[0].headers.size());
    for (std::size_t i = 0; i < first.translation_units[0].headers.size(); ++i) {
        CHECK(first.translation_units[0].headers[i].header_path ==
              second.translation_units[0].headers[i].header_path);
        CHECK(first.translation_units[0].headers[i].depth_kind ==
              second.translation_units[0].headers[i].depth_kind);
    }
    CHECK(first.include_node_budget_effective == second.include_node_budget_effective);
    CHECK(first.include_node_budget_reached == second.include_node_budget_reached);
}
