#include <doctest/doctest.h>

#include <cstdlib>
#include <sstream>
#include <string>

#include "adapters/cli/cli_adapter.h"
#include "adapters/cli/exit_codes.h"
#include "adapters/input/compile_commands_json_adapter.h"
#include "hexagon/services/project_analyzer.h"

namespace {

using xray::adapters::cli::CliAdapter;
using xray::adapters::cli::ExitCode;
using xray::adapters::input::CompileCommandsJsonAdapter;
using xray::hexagon::services::ProjectAnalyzer;

struct CliFixture {
    CompileCommandsJsonAdapter compile_database_adapter;
    ProjectAnalyzer project_analyzer{compile_database_adapter};
    CliAdapter cli{project_analyzer};
    std::ostringstream out;
    std::ostringstream err;

    int run(std::initializer_list<const char*> args) {
        std::vector<const char*> argv_vec = {"cmake-xray"};
        argv_vec.insert(argv_vec.end(), args);
        return cli.run(static_cast<int>(argv_vec.size()), argv_vec.data(), out, err);
    }
};

const std::string testdata = "tests/e2e/testdata/";

}  // namespace

// --- Help ---

TEST_CASE_FIXTURE(CliFixture, "main --help returns exit 0 with help on stdout") {
    CHECK(run({"--help"}) == ExitCode::success);
    CHECK(out.str().find("cmake-xray") != std::string::npos);
    CHECK(out.str().find("analyze") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "analyze --help returns exit 0 with help on stdout") {
    CHECK(run({"analyze", "--help"}) == ExitCode::success);
    CHECK(out.str().find("--compile-commands") != std::string::npos);
    CHECK(err.str().empty());
}

// --- Success path ---

TEST_CASE_FIXTURE(CliFixture, "valid compile_commands.json returns exit 0 with entry count") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "valid/compile_commands.json").c_str()}) == ExitCode::success);
    CHECK(out.str().find("compile database loaded: 1 entries") != std::string::npos);
    CHECK(err.str().empty());
}

TEST_CASE_FIXTURE(CliFixture, "only_command testdata returns exit 0") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "only_command/compile_commands.json").c_str()}) == ExitCode::success);
    CHECK(out.str().find("1 entries") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "command_and_arguments testdata prefers arguments") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "command_and_arguments/compile_commands.json").c_str()}) ==
          ExitCode::success);
    CHECK(out.str().find("1 entries") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "empty_arguments_with_command falls back to command") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "empty_arguments_with_command/compile_commands.json").c_str()}) ==
          ExitCode::success);
    CHECK(out.str().find("1 entries") != std::string::npos);
}

// --- CLI usage errors ---

TEST_CASE_FIXTURE(CliFixture, "missing --compile-commands returns exit 2") {
    CHECK(run({"analyze"}) == ExitCode::cli_usage_error);
}

// --- Input file errors ---

TEST_CASE_FIXTURE(CliFixture, "nonexistent file returns exit 3") {
    CHECK(run({"analyze", "--compile-commands", "/nonexistent/compile_commands.json"}) ==
          ExitCode::input_not_accessible);
    CHECK(err.str().find("cannot open") != std::string::npos);
    CHECK(err.str().find("hint:") != std::string::npos);
}

// --- Invalid input data ---

TEST_CASE_FIXTURE(CliFixture, "invalid JSON returns exit 4") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "invalid_syntax/compile_commands.json").c_str()}) ==
          ExitCode::input_invalid);
    CHECK(err.str().find("not valid JSON") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "root not an array returns exit 4") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "not_an_array/compile_commands.json").c_str()}) ==
          ExitCode::input_invalid);
    CHECK(err.str().find("not an array") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "empty array returns exit 4") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "empty/compile_commands.json").c_str()}) == ExitCode::input_invalid);
    CHECK(err.str().find("empty") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "missing fields returns exit 4 with diagnostics") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "missing_fields/compile_commands.json").c_str()}) ==
          ExitCode::input_invalid);
    CHECK(err.str().find("invalid entries") != std::string::npos);
    CHECK(err.str().find("entry 0") != std::string::npos);
}

TEST_CASE_FIXTURE(CliFixture, "mixed valid and invalid returns exit 4") {
    CHECK(run({"analyze", "--compile-commands",
               (testdata + "mixed_valid_invalid/compile_commands.json").c_str()}) ==
          ExitCode::input_invalid);
    CHECK(err.str().find("1 invalid entries") != std::string::npos);
}
