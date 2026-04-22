#include <doctest/doctest.h>

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>

#include "adapters/input/compile_commands_json_adapter.h"
#include "hexagon/model/compile_database_result.h"

namespace {

using xray::adapters::input::CompileCommandsJsonAdapter;
using xray::hexagon::model::CompileDatabaseError;

const std::string testdata = "tests/e2e/testdata/";

}  // namespace

// --- Success cases ---

TEST_CASE("valid compile_commands.json loads successfully") {
    const CompileCommandsJsonAdapter adapter;
    const auto result = adapter.load_compile_database(testdata + "valid/compile_commands.json");

    CHECK(result.is_success());
    REQUIRE(result.entries().size() == 1);
    CHECK(result.entries()[0].file() == "/project/src/main.cpp");
    CHECK(result.entries()[0].directory() == "/project/build");
    CHECK_FALSE(result.entries()[0].arguments().empty());
}

TEST_CASE("entry with only command field loads successfully") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "only_command/compile_commands.json");

    CHECK(result.is_success());
    REQUIRE(result.entries().size() == 1);
    CHECK(result.entries()[0].arguments().size() == 1);
    CHECK(result.entries()[0].arguments()[0] ==
          "g++ -std=c++20 -o main.cpp.o -c /project/src/main.cpp");
}

TEST_CASE("entry with command and arguments uses arguments") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "command_and_arguments/compile_commands.json");

    CHECK(result.is_success());
    REQUIRE(result.entries().size() == 1);
    CHECK(result.entries()[0].arguments().size() == 6);
    CHECK(result.entries()[0].arguments()[0] == "g++");
}

TEST_CASE("entry with empty arguments array falls back to command") {
    const CompileCommandsJsonAdapter adapter;
    const auto result = adapter.load_compile_database(
        testdata + "empty_arguments_with_command/compile_commands.json");

    CHECK(result.is_success());
    REQUIRE(result.entries().size() == 1);
    CHECK(result.entries()[0].arguments().size() == 1);
}

// --- Error cases ---

TEST_CASE("file not found returns file_not_accessible error") {
    const CompileCommandsJsonAdapter adapter;
    const auto result = adapter.load_compile_database("/nonexistent/compile_commands.json");

    CHECK_FALSE(result.is_success());
    CHECK(result.error() == CompileDatabaseError::file_not_accessible);
    CHECK(result.error_description().find("/nonexistent/") != std::string::npos);
}

TEST_CASE("syntactically invalid JSON returns invalid_json error") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "invalid_syntax/compile_commands.json");

    CHECK_FALSE(result.is_success());
    CHECK(result.error() == CompileDatabaseError::invalid_json);
}

TEST_CASE("root is not an array returns not_an_array error") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "not_an_array/compile_commands.json");

    CHECK_FALSE(result.is_success());
    CHECK(result.error() == CompileDatabaseError::not_an_array);
}

TEST_CASE("empty array returns empty_database error") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "empty/compile_commands.json");

    CHECK_FALSE(result.is_success());
    CHECK(result.error() == CompileDatabaseError::empty_database);
}

TEST_CASE("entries with missing fields return invalid_entries error") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "missing_fields/compile_commands.json");

    CHECK_FALSE(result.is_success());
    CHECK(result.error() == CompileDatabaseError::invalid_entries);
    CHECK(result.entries().empty());
    CHECK(result.entry_diagnostics().size() == 3);
}

TEST_CASE("mixed valid and invalid entries rejects entire database") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "mixed_valid_invalid/compile_commands.json");

    CHECK_FALSE(result.is_success());
    CHECK(result.error() == CompileDatabaseError::invalid_entries);
    CHECK(result.entries().empty());
    REQUIRE(result.entry_diagnostics().size() == 1);
    CHECK(result.entry_diagnostics()[0].index() == 1);
}

TEST_CASE("entry missing file field is diagnosed") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "missing_fields/compile_commands.json");

    REQUIRE(!result.entry_diagnostics().empty());
    CHECK(result.entry_diagnostics()[0].index() == 0);
    CHECK(result.entry_diagnostics()[0].message().find("\"file\"") != std::string::npos);
}

TEST_CASE("entry missing directory field is diagnosed") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "missing_fields/compile_commands.json");

    REQUIRE(result.entry_diagnostics().size() >= 2);
    CHECK(result.entry_diagnostics()[1].index() == 1);
    CHECK(result.entry_diagnostics()[1].message().find("\"directory\"") != std::string::npos);
}

TEST_CASE("entry with empty arguments and no command is invalid") {
    const CompileCommandsJsonAdapter adapter;
    const auto result =
        adapter.load_compile_database(testdata + "missing_fields/compile_commands.json");

    REQUIRE(result.entry_diagnostics().size() >= 3);
    CHECK(result.entry_diagnostics()[2].index() == 2);
    CHECK(result.entry_diagnostics()[2].message().find("\"command\"") != std::string::npos);
}

TEST_CASE("entry with empty arguments array and no command is rejected") {
    // Inline JSON: arguments is [], no command field — must fail with exit 4
    // This covers the testmatrix case from plan-M1.md:342 explicitly.
    const std::string path = "tests/e2e/testdata/empty_args_no_cmd.json";
    // Write inline test file
    {
        std::ofstream f(path);
        f << R"([{"file": "main.cpp", "directory": "/project", "arguments": []}])";
    }
    const CompileCommandsJsonAdapter adapter;
    const auto result = adapter.load_compile_database(path);
    std::remove(path.c_str());

    CHECK_FALSE(result.is_success());
    CHECK(result.error() == CompileDatabaseError::invalid_entries);
    REQUIRE(result.entry_diagnostics().size() == 1);
    CHECK(result.entry_diagnostics()[0].message().find("\"command\"") != std::string::npos);
}
