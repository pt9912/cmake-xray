#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "hexagon/services/compile_db_fingerprint.h"

TEST_CASE("compile-db project identity normalizes, sorts and deduplicates source paths") {
    const std::vector<std::string> paths{"src\\bar.cpp", "./src/foo.cpp",
                                         "src/foo.cpp/"};

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(paths) ==
          "compile-db:c06b16ff6cfee1f22827b1c3b1df7b648203da45b64d142afefa7533aa213c3e");
}

TEST_CASE("compile-db project identity is input-order independent") {
    const std::vector<std::string> sorted{"src/bar.cpp", "src/foo.cpp"};
    const std::vector<std::string> permuted{"src/foo.cpp", "src/bar.cpp"};

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(sorted) ==
          xray::hexagon::services::compile_db_project_identity_from_source_paths(permuted));
}

TEST_CASE("compile-db project identity escapes canonical JSON control characters") {
    const std::vector<std::string> paths{
        "src/quote\"file.cpp",
        "src/new\nline.cpp",
        "src/carriage\rreturn.cpp",
        "src/tab\tfile.cpp",
        "src/back\bfile.cpp",
        "src/form\ffile.cpp",
        std::string{"src/control"} + '\x01' + "file.cpp",
    };

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(paths) ==
          "compile-db:5b9573cef8958aab150226c584ca3195def9b0b40584ab19b8cfba0cd1d757a0");
}

TEST_CASE("compile-db project identity keeps accepted build-context trade-off") {
    const std::vector<std::string> first_context{"src/main.cpp", "src/lib.cpp"};
    const std::vector<std::string> second_context{"src/lib.cpp", "src/main.cpp",
                                                  "src/main.cpp"};

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(first_context) ==
          xray::hexagon::services::compile_db_project_identity_from_source_paths(second_context));
}

TEST_CASE("compile-db project identity ignores empty source paths") {
    const std::vector<std::string> empty_paths{"", "."};

    CHECK(xray::hexagon::services::normalize_project_identity_path("") == "");
    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(empty_paths) ==
          "");
}

TEST_CASE("compile-db project identity strips shared absolute checkout prefixes") {
    const std::vector<std::string> first_checkout{
        "/home/user/cmake-xray/src/main.cpp",
        "/home/user/cmake-xray/src/lib.cpp",
    };
    const std::vector<std::string> second_checkout{
        "/workspace/cmake-xray/src/lib.cpp",
        "/workspace/cmake-xray/src/main.cpp",
    };

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(first_checkout) ==
          "compile-db:822ff9e3fb10e105a4c439080c6dd645cf1b4bf853dc4ad0b43336dacabdc82c");
    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(first_checkout) ==
          xray::hexagon::services::compile_db_project_identity_from_source_paths(second_checkout));
}

TEST_CASE("compile-db project identity stabilizes mixed relative and absolute paths") {
    const std::vector<std::string> first_checkout{
        "gen/generated.cpp",
        "/home/user/cmake-xray/src/main.cpp",
    };
    const std::vector<std::string> second_checkout{
        "/workspace/cmake-xray/src/main.cpp",
        "gen/generated.cpp",
    };

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(first_checkout) ==
          "compile-db:1beb28bb485d513729913b5b385ce9d027670f35a2fa5840ea9e3d5db94ec689");
    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(first_checkout) ==
          xray::hexagon::services::compile_db_project_identity_from_source_paths(second_checkout));
}

TEST_CASE("compile-db project identity keeps root anchors in stabilized paths") {
    const std::vector<std::string> paths{
        "\\\\server\\share\\repo\\src\\main.cpp",
        "/server/share/repo/src/main.cpp",
    };

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(paths) ==
          "compile-db:dd5ca6c6cecfc99f1ae4b4ba2ec8fa346048b2152e0c2265123194d90e1b8b86");
}

TEST_CASE("compile-db project identity keeps drive root anchors in stabilized paths") {
    const std::vector<std::string> paths{
        "C:\\repo\\src\\main.cpp",
        "/repo/src/main.cpp",
    };

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(paths) ==
          "compile-db:8b92861d606565bac13518c82f44c413aa8c984121095a181bdeec324c65001a");
}

TEST_CASE("compile-db project identity keeps root-level absolute paths without prefix") {
    const std::vector<std::string> paths{
        "/main.cpp",
        "/lib.cpp",
    };

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(paths) ==
          "compile-db:822ff9e3fb10e105a4c439080c6dd645cf1b4bf853dc4ad0b43336dacabdc82c");
}

TEST_CASE("compile-db project identity avoids relative suffix collision") {
    const std::vector<std::string> paths{
        "main.cpp",
        "/home/user/cmake-xray/src/main.cpp",
    };

    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(paths) !=
          xray::hexagon::services::compile_db_project_identity_from_source_paths(
              {"main.cpp"}));
}

TEST_CASE("project identity path normalization handles windows drive letters") {
    CHECK(xray::hexagon::services::normalize_project_identity_path(
              "C:\\Repo\\.\\src\\foo.cpp\\") == "c:/Repo/src/foo.cpp");
}

TEST_CASE("project identity path normalization preserves UNC roots") {
    CHECK(xray::hexagon::services::normalize_project_identity_path(
              "\\\\server\\share\\.\\repo\\") == "//server/share/repo");
}
