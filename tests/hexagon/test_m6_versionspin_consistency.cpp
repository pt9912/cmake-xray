#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "hexagon/model/application_info.h"
#include "hexagon/model/compare_result.h"
#include "hexagon/model/report_format_version.h"

namespace {

constexpr std::string_view kM6ReleaseVersion{"1.3.0"};
constexpr int kM6AnalyzeFormatVersion = 6;
constexpr int kM6CompareFormatVersion = 1;

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in.is_open(), "cannot open " << path.string());
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

nlohmann::json read_json(const std::filesystem::path& path) {
    return nlohmann::json::parse(read_text(path));
}

void require_no_legacy_project_version(const std::filesystem::path& path) {
    CAPTURE(path.string());
    CHECK(read_text(path).find("1.2.0") == std::string::npos);
}

void require_tree_without_legacy_project_version(const std::filesystem::path& root) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        require_no_legacy_project_version(entry.path());
    }
}

}  // namespace

TEST_CASE("m6_versionspin_consistency") {
    CHECK(xray::hexagon::model::application_info().version == kM6ReleaseVersion);
    CHECK(std::string_view{XRAY_APP_VERSION_STRING} == kM6ReleaseVersion);
    CHECK(std::string_view{XRAY_APP_VERSION_STRING}.starts_with("v") == false);

    CHECK(xray::hexagon::model::kReportFormatVersion == kM6AnalyzeFormatVersion);
    CHECK(xray::hexagon::model::kCompareFormatVersion == kM6CompareFormatVersion);

    const auto report_schema = read_json(XRAY_REPORT_JSON_SCHEMA_PATH);
    CHECK(report_schema["$defs"]["FormatVersion"]["const"].get<int>() ==
          xray::hexagon::model::kReportFormatVersion);

    const auto compare_schema = read_json(XRAY_REPORT_COMPARE_SCHEMA_PATH);
    CHECK(compare_schema["properties"]["format_version"]["const"].get<int>() ==
          xray::hexagon::model::kCompareFormatVersion);

    const auto project_root = std::filesystem::path{XRAY_PROJECT_SOURCE_DIR};
    require_no_legacy_project_version(project_root / "CMakeLists.txt");
    require_tree_without_legacy_project_version(project_root / "src");
    require_tree_without_legacy_project_version(project_root / "cmake");
}
