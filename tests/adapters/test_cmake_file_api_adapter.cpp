#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <system_error>

#include <sys/capability.h>

#include "adapters/input/cmake_file_api_adapter.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/services/analysis_support.h"

namespace {

using xray::adapters::input::CmakeFileApiAdapter;
using xray::adapters::input::CompileCommandsJsonAdapter;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::TargetMetadataStatus;

const std::string testdata = "tests/e2e/testdata/m4/";

struct ScopedDropDacCapabilities {
    ScopedDropDacCapabilities() {
        cap_t caps = cap_get_proc();
        cap_value_t cap_list[] = {CAP_DAC_READ_SEARCH, CAP_DAC_OVERRIDE};
        cap_set_flag(caps, CAP_EFFECTIVE, 2, cap_list, CAP_CLEAR);
        cap_set_proc(caps);
        cap_free(caps);
    }

    ~ScopedDropDacCapabilities() {
        cap_t caps = cap_get_proc();
        cap_value_t cap_list[] = {CAP_DAC_READ_SEARCH, CAP_DAC_OVERRIDE};
        cap_set_flag(caps, CAP_EFFECTIVE, 2, cap_list, CAP_SET);
        cap_set_proc(caps);
        cap_free(caps);
    }

    ScopedDropDacCapabilities(const ScopedDropDacCapabilities&) = delete;
    ScopedDropDacCapabilities& operator=(const ScopedDropDacCapabilities&) = delete;
};

}  // namespace

// --- Success cases ---

TEST_CASE("file api adapter loads valid reply from build directory") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "file_api_only/build");

    CHECK(result.compile_database.is_success());
    CHECK(result.source == ObservationSource::derived);
    CHECK(result.target_metadata == TargetMetadataStatus::loaded);
    CHECK(result.source_root == "/project");
    REQUIRE(result.compile_database.entries().size() == 2);
}

TEST_CASE("file api adapter produces identical results from build dir and reply dir") {
    const CmakeFileApiAdapter adapter;
    const auto from_build = adapter.load_build_model(testdata + "file_api_only/build");
    const auto from_reply =
        adapter.load_build_model(testdata + "file_api_only/build/.cmake/api/v1/reply");

    REQUIRE(from_build.compile_database.is_success());
    REQUIRE(from_reply.compile_database.is_success());
    REQUIRE(from_build.compile_database.entries().size() ==
            from_reply.compile_database.entries().size());

    for (std::size_t i = 0; i < from_build.compile_database.entries().size(); ++i) {
        CHECK(from_build.compile_database.entries()[i].file() ==
              from_reply.compile_database.entries()[i].file());
        CHECK(from_build.compile_database.entries()[i].directory() ==
              from_reply.compile_database.entries()[i].directory());
        CHECK(from_build.compile_database.entries()[i].arguments().size() ==
              from_reply.compile_database.entries()[i].arguments().size());
    }

    CHECK(from_build.source_root == from_reply.source_root);
    REQUIRE(from_build.target_assignments.size() == from_reply.target_assignments.size());
    for (std::size_t i = 0; i < from_build.target_assignments.size(); ++i) {
        CHECK(from_build.target_assignments[i].observation_key ==
              from_reply.target_assignments[i].observation_key);
    }
}

TEST_CASE("file api adapter extracts target assignments") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "file_api_only/build");

    REQUIRE(result.target_assignments.size() == 2);
    // Each source belongs to exactly one target
    CHECK(result.target_assignments[0].targets.size() == 1);
    CHECK(result.target_assignments[1].targets.size() == 1);
}

TEST_CASE("file api adapter synthesizes entries with correct include and define counts") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "file_api_only/build");

    REQUIRE(result.compile_database.entries().size() == 2);
    // app target: 2 includes (-I + -isystem) + 1 define + compiler + -c + source = 8 args
    // core target: 1 include + compiler + -c + source = 5 args
    bool found_app = false;
    bool found_core = false;
    for (const auto& entry : result.compile_database.entries()) {
        if (entry.file().find("main.cpp") != std::string::npos) {
            // c++ -I /project/include -isystem /usr/include -DAPP_VERSION=1 -c <source>
            CHECK(entry.arguments().size() == 8);
            found_app = true;
        }
        if (entry.file().find("core.cpp") != std::string::npos) {
            // c++ -I /project/include -c <source>
            CHECK(entry.arguments().size() == 5);
            found_core = true;
        }
    }
    CHECK(found_app);
    CHECK(found_core);
}

TEST_CASE("file api adapter assigns shared source to multiple targets with correct types") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "multi_target/build");

    CHECK(result.compile_database.is_success());
    // main.cpp (app only) + shared.cpp (app) + shared.cpp (core) = 3 entries
    CHECK(result.compile_database.entries().size() == 3);

    // All three entries produce valid observations downstream
    const auto& first_entry = result.compile_database.entries()[0];
    const auto dummy_compile_commands = first_entry.directory() + "/compile_commands.json";
    const auto observations =
        xray::hexagon::services::build_translation_unit_observations(
            result.compile_database.entries(), dummy_compile_commands);
    CHECK(observations.size() == 3);

    // Find the assignment for shared.cpp
    bool found_shared_assignment = false;
    for (const auto& assignment : result.target_assignments) {
        if (assignment.observation_key.find("shared.cpp") != std::string::npos) {
            // shared.cpp in same directoryIndex -> one key, two targets
            REQUIRE(assignment.targets.size() == 2);
            // Sorted deterministically: app before core
            CHECK(assignment.targets[0].display_name == "app");
            CHECK(assignment.targets[0].type == "EXECUTABLE");
            CHECK(assignment.targets[1].display_name == "core");
            CHECK(assignment.targets[1].type == "STATIC_LIBRARY");
            found_shared_assignment = true;
        }
    }
    CHECK(found_shared_assignment);
}

TEST_CASE("file api adapter skips sources without compile group index") {
    const CmakeFileApiAdapter adapter;
    // partial_targets has 2 targets: app (1 source with compileGroupIndex) and
    // iface (1 header-only source without compileGroupIndex)
    const auto result = adapter.load_build_model(testdata + "partial_targets/build");

    CHECK(result.compile_database.is_success());
    // Only app's source produces an entry; iface's header is skipped
    CHECK(result.compile_database.entries().size() == 1);
    CHECK(result.compile_database.entries()[0].file().find("main.cpp") != std::string::npos);
    // iface target produced no entries, so metadata is partial
    CHECK(result.target_metadata == TargetMetadataStatus::partial);
}

// --- Reply selection ---

TEST_CASE("file api adapter selects lexicographically latest index among multiple") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "multiple_indexes/build");

    CHECK(result.compile_database.is_success());
    // The newer index (2025-06-15) points to a valid codemodel
    CHECK(result.compile_database.entries().size() == 1);
}

// --- Error cases ---

TEST_CASE("file api adapter returns file_api_not_accessible for unreadable reply directory") {
    const auto unreadable_dir =
        std::filesystem::temp_directory_path() / "cmake-xray-unreadable-reply";
    const auto reply_dir = unreadable_dir / ".cmake" / "api" / "v1" / "reply";
    std::filesystem::create_directories(reply_dir);
    std::filesystem::permissions(reply_dir, std::filesystem::perms::none);

    const CmakeFileApiAdapter adapter;
    xray::hexagon::model::BuildModelResult result;
    {
        const ScopedDropDacCapabilities guard;
        result = adapter.load_build_model(unreadable_dir.string());
    }

    std::filesystem::permissions(reply_dir, std::filesystem::perms::all);
    std::error_code ec;
    std::filesystem::remove_all(unreadable_dir, ec);

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_not_accessible);
    CHECK(result.compile_database.error_description().find("cannot read") != std::string::npos);
}

TEST_CASE("file api adapter returns file_api_not_accessible for nonexistent path") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model("/nonexistent/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_not_accessible);
    CHECK(result.source == ObservationSource::derived);
}

TEST_CASE("file api adapter returns file_api_invalid for invalid index JSON") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "invalid_file_api/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
}

TEST_CASE("file api adapter returns file_api_invalid for missing target reply after retry") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "missing_target_reply/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("no newer index") !=
          std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid when error reply is newer than index") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "error_reply/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("error reply") != std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid for empty reply directory") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "empty_reply/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("no index or error") !=
          std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid when index has no codemodel") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "no_codemodel/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("codemodel") != std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid for invalid codemodel JSON") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "invalid_codemodel/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("not valid JSON") !=
          std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid for codemodel without paths") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "missing_paths/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("paths") != std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid for codemodel without configurations") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "no_configurations/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("no configurations") !=
          std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid for multi-config codemodel") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "multi_config/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("2 configurations") !=
          std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid for codemodel with empty targets") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "empty_targets/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("no targets") != std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid for structurally invalid reply data") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "type_error_reply/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("unexpected structure") !=
          std::string::npos);
}

TEST_CASE("file api adapter returns file_api_invalid when targets have no compilable sources") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "no_compilable_sources/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("no compilable sources") !=
          std::string::npos);
}

TEST_CASE("file api adapter reports partial target metadata when some targets lack sources") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "partial_targets/build");

    CHECK(result.compile_database.is_success());
    CHECK(result.target_metadata == TargetMetadataStatus::partial);
    CHECK(result.compile_database.entries().size() == 1);
    REQUIRE(!result.diagnostics.empty());
    CHECK(result.diagnostics[0].message.find("1 of 2 targets") != std::string::npos);
}

// --- Cross-adapter key consistency ---

TEST_CASE("file api adapter target_assignment keys match downstream observation keys") {
    // The File API adapter computes observation_key = resolved_source|resolved_directory.
    // Downstream, analysis_support builds unique_key from CompileEntry via the same
    // normalization. The keys must match so that target assignments can be looked up.
    const CmakeFileApiAdapter file_api_adapter;
    const auto result = file_api_adapter.load_build_model(testdata + "file_api_only/build");

    REQUIRE(result.compile_database.is_success());
    REQUIRE(!result.compile_database.entries().empty());
    REQUIRE(!result.target_assignments.empty());

    // File API entries use absolute paths for file and directory.
    // build_translation_unit_observations needs a compile_commands_path to derive
    // the base directory; for absolute-path entries we pass a dummy path whose
    // parent equals the entry directory so that resolve_path is a no-op.
    const auto& first_entry = result.compile_database.entries()[0];
    const auto dummy_compile_commands =
        first_entry.directory() + "/compile_commands.json";

    const auto observations =
        xray::hexagon::services::build_translation_unit_observations(
            result.compile_database.entries(), dummy_compile_commands);

    for (const auto& assignment : result.target_assignments) {
        bool key_found = false;
        for (const auto& obs : observations) {
            if (obs.reference.unique_key == assignment.observation_key) {
                key_found = true;
                break;
            }
        }
        CHECK_MESSAGE(key_found, "target assignment key must match an observation key: ",
                      assignment.observation_key);
    }
}

// --- Determinism ---

TEST_CASE("file api adapter target assignments are sorted deterministically") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "file_api_only/build");

    REQUIRE(result.target_assignments.size() >= 2);
    for (std::size_t i = 1; i < result.target_assignments.size(); ++i) {
        CHECK(result.target_assignments[i - 1].observation_key <
              result.target_assignments[i].observation_key);
    }
}

TEST_CASE("file api adapter produces identical results for permuted target order") {
    const CmakeFileApiAdapter adapter;
    const auto baseline = adapter.load_build_model(testdata + "file_api_only/build");
    const auto permuted = adapter.load_build_model(testdata + "permuted_targets/build");

    REQUIRE(baseline.compile_database.is_success());
    REQUIRE(permuted.compile_database.is_success());

    REQUIRE(baseline.compile_database.entries().size() ==
            permuted.compile_database.entries().size());
    for (std::size_t i = 0; i < baseline.compile_database.entries().size(); ++i) {
        CHECK(baseline.compile_database.entries()[i].file() ==
              permuted.compile_database.entries()[i].file());
        CHECK(baseline.compile_database.entries()[i].directory() ==
              permuted.compile_database.entries()[i].directory());
    }

    REQUIRE(baseline.target_assignments.size() == permuted.target_assignments.size());
    for (std::size_t i = 0; i < baseline.target_assignments.size(); ++i) {
        CHECK(baseline.target_assignments[i].observation_key ==
              permuted.target_assignments[i].observation_key);
    }
}
