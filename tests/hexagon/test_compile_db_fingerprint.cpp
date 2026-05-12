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

TEST_CASE("project identity path normalization handles windows drive letters") {
    CHECK(xray::hexagon::services::normalize_project_identity_path(
              "C:\\Repo\\.\\src\\foo.cpp\\") == "c:/Repo/src/foo.cpp");
}
