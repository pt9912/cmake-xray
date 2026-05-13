#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "adapters/output/json_compare_adapter.h"
#include "hexagon/model/compare_result.h"

namespace {

using xray::adapters::output::JsonCompareAdapter;
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
using xray::hexagon::model::kCompareFormatVersion;

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

CompareScalarValue scalar_array(std::string lhs, std::string rhs) {
    CompareScalarArray values;
    values.push_back(scalar_string(std::move(lhs)));
    values.push_back(scalar_string(std::move(rhs)));
    CompareScalarValue out;
    out.value = std::move(values);
    return out;
}

CompareResult compare_result() {
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

nlohmann::json parse(const std::string& text) {
    return nlohmann::json::parse(text);
}

}  // namespace

TEST_CASE("json compare adapter writes top-level contract and inputs") {
    const JsonCompareAdapter adapter;
    const auto report = adapter.write_compare_report(compare_result());
    REQUIRE(!report.empty());
    CHECK(report.back() == '\n');
    const auto doc = parse(report);

    CHECK(doc["format"] == "cmake-xray.compare");
    CHECK(doc["format_version"] == kCompareFormatVersion);
    CHECK(doc["report_type"] == "compare");
    CHECK(doc["inputs"]["baseline_path"] == "baseline.json");
    CHECK(doc["inputs"]["current_path"] == "current.json");
    CHECK(doc["inputs"]["project_identity"] == "compile-db:same");
    CHECK(doc["inputs"]["project_identity_source"] ==
          "fallback_compile_database_fingerprint");
}

TEST_CASE("json compare adapter groups diffs by kind with changed fields") {
    const JsonCompareAdapter adapter;
    const auto doc = parse(adapter.write_compare_report(compare_result()));

    const auto& tu = doc["diffs"]["translation_units"];
    REQUIRE(tu["added"].size() == 1);
    REQUIRE(tu["removed"].size() == 1);
    REQUIRE(tu["changed"].size() == 1);
    CHECK(tu["added"][0]["source_path"] == "src/new.cpp");
    CHECK(tu["removed"][0]["source_path"] == "src/old.cpp");
    CHECK(tu["changed"][0]["changes"][0]["field"] == "metrics.score");
    CHECK(tu["changed"][0]["changes"][0]["baseline_value"] == 10);
    CHECK(tu["changed"][0]["changes"][0]["current_value"] == 13);
    CHECK(tu["changed"][0]["changes"][1]["baseline_value"][0] == "app");

    CHECK(doc["diffs"]["include_hotspots"]["changed"][0]["header_path"] ==
          "include/config.h");
    CHECK(doc["diffs"]["target_nodes"]["changed"][0]["unique_key"] ==
          "app::EXECUTABLE");
    CHECK(doc["diffs"]["target_edges"]["added"][0]["from_unique_key"] == "app");
    CHECK(doc["diffs"]["target_hubs"]["removed"][0]["direction"] == "inbound");
}

TEST_CASE("json compare adapter writes diagnostics object") {
    const JsonCompareAdapter adapter;
    const auto doc = parse(adapter.write_compare_report(compare_result()));

    const auto& diagnostics = doc["diagnostics"];
    CHECK(diagnostics["configuration_drifts"][0]["field"] ==
          "include_filter.include_scope");
    CHECK(diagnostics["configuration_drifts"][0]["severity"] == "warning");
    CHECK(diagnostics["configuration_drifts"][0]["ci_policy_hint"] ==
          "review_required");
    CHECK(diagnostics["data_availability_drifts"][0]["section"] == "target_graph");
    CHECK(diagnostics["project_identity_drift"]["baseline_project_identity"] ==
          "compile-db:old");
    CHECK(diagnostics["project_identity_drift"]["shared_source_path_count"] == 2);
}

TEST_CASE("json compare adapter renders null identity and null identity drift") {
    auto result = compare_result();
    result.inputs.project_identity = std::nullopt;
    result.project_identity_drift = std::nullopt;

    const JsonCompareAdapter adapter;
    const auto doc = parse(adapter.write_compare_report(result));

    CHECK(doc["inputs"]["project_identity"].is_null());
    CHECK(doc["diagnostics"]["project_identity_drift"].is_null());
}
