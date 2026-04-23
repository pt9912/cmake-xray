#include <doctest/doctest.h>

#include <string>

#include "adapters/input/cmake_file_api_adapter.h"
#include "hexagon/model/compile_database_result.h"
#include "hexagon/model/observation_source.h"

namespace {

using xray::adapters::input::CmakeFileApiAdapter;
using xray::hexagon::model::CompileDatabaseError;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::TargetMetadataStatus;

const std::string testdata = "tests/e2e/testdata/m4/";

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

TEST_CASE("file api adapter loads valid reply from reply directory directly") {
    const CmakeFileApiAdapter adapter;
    const auto result =
        adapter.load_build_model(testdata + "file_api_only/build/.cmake/api/v1/reply");

    CHECK(result.compile_database.is_success());
    REQUIRE(result.compile_database.entries().size() == 2);
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

TEST_CASE("file api adapter assigns shared source to multiple targets") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "multi_target/build");

    CHECK(result.compile_database.is_success());
    // main.cpp (app only) + shared.cpp (app) + shared.cpp (core) = 3 entries
    CHECK(result.compile_database.entries().size() == 3);

    // Find the assignment for shared.cpp
    bool found_shared_assignment = false;
    for (const auto& assignment : result.target_assignments) {
        if (assignment.observation_key.find("shared.cpp") != std::string::npos) {
            // shared.cpp in same directoryIndex -> one key, two targets
            CHECK(assignment.targets.size() == 2);
            found_shared_assignment = true;
        }
    }
    CHECK(found_shared_assignment);
}

TEST_CASE("file api adapter skips sources without compile group index") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "file_api_only/build");

    // Only sources with compileGroupIndex produce entries
    CHECK(result.compile_database.is_success());
    for (const auto& entry : result.compile_database.entries()) {
        CHECK_FALSE(entry.file().empty());
    }
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
    CHECK(result.compile_database.error_description().find("not accessible") !=
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

TEST_CASE("file api adapter returns file_api_invalid when targets have no compilable sources") {
    const CmakeFileApiAdapter adapter;
    const auto result = adapter.load_build_model(testdata + "no_compilable_sources/build");

    CHECK_FALSE(result.compile_database.is_success());
    CHECK(result.compile_database.error() == CompileDatabaseError::file_api_invalid);
    CHECK(result.compile_database.error_description().find("no compilable sources") !=
          std::string::npos);
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
