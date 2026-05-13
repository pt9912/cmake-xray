#include <doctest/doctest.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "adapters/output/console_compare_adapter.h"
#include "adapters/output/markdown_compare_adapter.h"
#include "hexagon/model/compare_result.h"

namespace {

using xray::adapters::output::ConsoleCompareAdapter;
using xray::adapters::output::MarkdownCompareAdapter;
using xray::hexagon::model::CompareDiagnosticConfigurationDrift;
using xray::hexagon::model::CompareDiffKind;
using xray::hexagon::model::CompareFieldChange;
using xray::hexagon::model::CompareResult;
using xray::hexagon::model::CompareScalarArray;
using xray::hexagon::model::CompareScalarValue;
using xray::hexagon::model::IncludeHotspotDiff;
using xray::hexagon::model::TargetEdgeDiff;
using xray::hexagon::model::TargetHubDiff;
using xray::hexagon::model::TargetNodeDiff;
using xray::hexagon::model::TranslationUnitDiff;

CompareScalarValue scalar_string(std::string value) {
    CompareScalarValue out;
    out.value = std::move(value);
    return out;
}

CompareScalarValue scalar_int(std::int64_t value) {
    CompareScalarValue out;
    out.value = value;
    return out;
}

CompareScalarValue scalar_bool(bool value) {
    CompareScalarValue out;
    out.value = value;
    return out;
}

CompareScalarValue scalar_null() {
    return {};
}

CompareScalarValue scalar_array(std::string lhs, std::string rhs) {
    CompareScalarArray values;
    values.push_back(scalar_string(std::move(lhs)));
    values.push_back(scalar_string(std::move(rhs)));
    CompareScalarValue out;
    out.value = std::move(values);
    return out;
}

CompareResult typical_compare_result() {
    CompareResult result;
    result.inputs.baseline_path = "baseline.json";
    result.inputs.current_path = "current.json";
    result.inputs.baseline_format_version = 6;
    result.inputs.current_format_version = 6;
    result.inputs.project_identity = "compile-db:same";
    result.inputs.project_identity_source = "fallback_compile_database_fingerprint";
    result.summary.translation_units_added = 1;
    result.summary.translation_units_removed = 1;
    result.summary.translation_units_changed = 1;
    result.summary.include_hotspots_changed = 1;
    result.summary.target_nodes_changed = 1;
    result.summary.target_edges_added = 1;
    result.summary.target_hubs_removed = 1;
    result.translation_unit_diffs = {
        TranslationUnitDiff{CompareDiffKind::added, "src/new.cpp", "build", {}},
        TranslationUnitDiff{CompareDiffKind::removed, "src/old.cpp", "build", {}},
        TranslationUnitDiff{
            CompareDiffKind::changed,
            "src/app.cpp",
            "build",
            {CompareFieldChange{"metrics.score", scalar_int(10), scalar_int(13)},
             CompareFieldChange{"targets", scalar_array("app", "core"),
                                scalar_array("app", "ui")}}},
    };
    result.include_hotspot_diffs = {
        IncludeHotspotDiff{CompareDiffKind::changed,
                           "include/config.h",
                           "project",
                           "direct",
                           {CompareFieldChange{"affected_total_count", scalar_int(2),
                                               scalar_int(3)}}},
    };
    result.target_node_diffs = {
        TargetNodeDiff{CompareDiffKind::changed,
                       "app::EXECUTABLE",
                       {CompareFieldChange{"display_name", scalar_string("app"),
                                           scalar_string("app-renamed")}}},
    };
    result.target_edge_diffs = {
        TargetEdgeDiff{CompareDiffKind::added, "app", "core", "direct", {}},
    };
    result.target_hub_diffs = {
        TargetHubDiff{CompareDiffKind::removed, "legacy", "inbound"},
    };
    result.configuration_drifts = {
        CompareDiagnosticConfigurationDrift{
            "include_filter.include_scope",
            scalar_string("all"),
            scalar_string("project"),
        },
    };
    result.data_availability_drifts = {{"target_graph", "loaded", "partial"}};
    result.project_identity_drift =
        xray::hexagon::model::CompareDiagnosticProjectIdentityDrift{
            "compile-db:old", "compile-db:new", 2, 3, 2};
    return result;
}

std::string read_text_file(const std::string& path) {
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>{input},
                       std::istreambuf_iterator<char>{});
}

}  // namespace

TEST_CASE("console compare adapter writes the typical golden") {
    const ConsoleCompareAdapter adapter;
    CHECK(adapter.write_compare_report(typical_compare_result()) ==
          read_text_file("tests/e2e/testdata/m6/compare-reports/console/"
                         "compare-typical.txt"));
}

TEST_CASE("markdown compare adapter writes the typical golden") {
    const MarkdownCompareAdapter adapter;
    CHECK(adapter.write_compare_report(typical_compare_result()) ==
          read_text_file("tests/e2e/testdata/m6/compare-reports/markdown/"
                         "compare-typical.md"));
}

TEST_CASE("text compare adapters render null identity as drift allowed") {
    auto result = typical_compare_result();
    result.inputs.project_identity = std::nullopt;
    result.project_identity_drift = std::nullopt;

    const ConsoleCompareAdapter console;
    const MarkdownCompareAdapter markdown;

    CHECK(console.write_compare_report(result).find("project_identity: drift allowed") !=
          std::string::npos);
    CHECK(markdown.write_compare_report(result).find("Project identity: drift allowed") !=
          std::string::npos);
}

TEST_CASE("text compare adapters render boolean and null scalar values") {
    auto result = typical_compare_result();
    result.translation_unit_diffs = {
        TranslationUnitDiff{CompareDiffKind::changed,
                            "src/app.cpp",
                            "build",
                            {CompareFieldChange{"feature.enabled", scalar_bool(true),
                                                scalar_null()}}},
    };

    const ConsoleCompareAdapter console;
    const MarkdownCompareAdapter markdown;

    CHECK(console.write_compare_report(result).find("feature.enabled: true -> null") !=
          std::string::npos);
    CHECK(markdown.write_compare_report(result).find(
              "| `feature.enabled` | `true` | `null` |") != std::string::npos);
}

TEST_CASE("markdown compare adapter escapes code spans in changed values") {
    auto result = typical_compare_result();
    result.translation_unit_diffs = {
        TranslationUnitDiff{
            CompareDiffKind::changed,
            "src/app.cpp",
            "build",
            {CompareFieldChange{"targets", scalar_string("app|core"),
                                scalar_string("app``ui")}}},
    };

    const MarkdownCompareAdapter markdown;
    const auto report = markdown.write_compare_report(result);

    CHECK(report.find("`app\\|core`") != std::string::npos);
    CHECK(report.find("``` app``ui ```") != std::string::npos);
}
