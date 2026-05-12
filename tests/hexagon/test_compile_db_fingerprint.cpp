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
          "compile-db:6afc7a7213bdd1ebdb9fcf68bbbccb056f44c6b2887388e508163c8421691464");
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
          "compile-db:93a818b6f77b718f641cb3d6b13aaa5e2c64775fe9f872cffc59e73688df7bb8");
    CHECK(xray::hexagon::services::compile_db_project_identity_from_source_paths(first_checkout) ==
          xray::hexagon::services::compile_db_project_identity_from_source_paths(second_checkout));
}

TEST_CASE("compile-db project identity keeps original paths on relative suffix collision") {
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
