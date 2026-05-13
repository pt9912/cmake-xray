#include "adapters/output/json_compare_adapter.h"

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace xray::adapters::output {

namespace {

using nlohmann::ordered_json;
using xray::hexagon::model::CompareDiffKind;
using xray::hexagon::model::CompareFieldChange;
using xray::hexagon::model::CompareResult;
using xray::hexagon::model::CompareScalarArray;
using xray::hexagon::model::CompareScalarValue;
using xray::hexagon::model::kCompareFormatVersion;

std::string diff_kind_text(CompareDiffKind kind) {
    if (kind == CompareDiffKind::added) return "added";
    if (kind == CompareDiffKind::removed) return "removed";
    return "changed";
}

ordered_json render_scalar(const CompareScalarValue& value);

ordered_json render_scalar_array(const CompareScalarArray& values) {
    auto out = ordered_json::array();
    for (const auto& value : values) out.push_back(render_scalar(value));
    return ordered_json(std::move(out));
}

ordered_json render_scalar(const CompareScalarValue& value) {
    if (std::holds_alternative<std::string>(value.value)) {
        return std::get<std::string>(value.value);
    }
    if (std::holds_alternative<std::int64_t>(value.value)) {
        return std::get<std::int64_t>(value.value);
    }
    if (std::holds_alternative<bool>(value.value)) {
        return std::get<bool>(value.value);
    }
    if (std::holds_alternative<CompareScalarArray>(value.value)) {
        return render_scalar_array(std::get<CompareScalarArray>(value.value));
    }
    return nullptr;
}

ordered_json render_changes(const std::vector<CompareFieldChange>& changes) {
    auto out = ordered_json::array();
    for (const auto& change : changes) {
        ordered_json item;
        item["field"] = change.field;
        item["baseline_value"] = render_scalar(change.baseline_value);
        item["current_value"] = render_scalar(change.current_value);
        out.push_back(std::move(item));
    }
    return ordered_json(std::move(out));
}

ordered_json render_translation_unit_diff(
    const xray::hexagon::model::TranslationUnitDiff& diff) {
    ordered_json out;
    out["source_path"] = diff.source_path;
    out["build_context_key"] = diff.build_context_key;
    if (diff.kind == CompareDiffKind::changed) out["changes"] = render_changes(diff.changes);
    return ordered_json(std::move(out));
}

ordered_json render_include_hotspot_diff(
    const xray::hexagon::model::IncludeHotspotDiff& diff) {
    ordered_json out;
    out["header_path"] = diff.header_path;
    out["include_origin"] = diff.include_origin;
    out["include_depth_kind"] = diff.include_depth_kind;
    if (diff.kind == CompareDiffKind::changed) out["changes"] = render_changes(diff.changes);
    return ordered_json(std::move(out));
}

ordered_json render_target_node_diff(const xray::hexagon::model::TargetNodeDiff& diff) {
    ordered_json out;
    out["unique_key"] = diff.unique_key;
    if (diff.kind == CompareDiffKind::changed) out["changes"] = render_changes(diff.changes);
    return ordered_json(std::move(out));
}

ordered_json render_target_edge_diff(const xray::hexagon::model::TargetEdgeDiff& diff) {
    ordered_json out;
    out["from_unique_key"] = diff.from_unique_key;
    out["to_unique_key"] = diff.to_unique_key;
    out["kind"] = diff.kind_text;
    if (diff.kind == CompareDiffKind::changed) out["changes"] = render_changes(diff.changes);
    return ordered_json(std::move(out));
}

ordered_json render_target_hub_diff(const xray::hexagon::model::TargetHubDiff& diff) {
    ordered_json out;
    out["unique_key"] = diff.unique_key;
    out["direction"] = diff.direction;
    return ordered_json(std::move(out));
}

template <typename Diff, typename Renderer>
ordered_json render_diff_group(const std::vector<Diff>& diffs, Renderer renderer) {
    ordered_json out;
    out["added"] = ordered_json::array();
    out["removed"] = ordered_json::array();
    out["changed"] = ordered_json::array();
    for (const auto& diff : diffs) {
        out[diff_kind_text(diff.kind)].push_back(renderer(diff));
    }
    return ordered_json(std::move(out));
}

ordered_json render_inputs(const CompareResult& result) {
    ordered_json out;
    out["baseline_path"] = result.inputs.baseline_path;
    out["current_path"] = result.inputs.current_path;
    out["baseline_format_version"] = result.inputs.baseline_format_version;
    out["current_format_version"] = result.inputs.current_format_version;
    if (result.inputs.project_identity.has_value()) {
        out["project_identity"] = *result.inputs.project_identity;
    } else {
        out["project_identity"] = nullptr;
    }
    out["project_identity_source"] = result.inputs.project_identity_source;
    return ordered_json(std::move(out));
}

ordered_json render_summary(const CompareResult& result) {
    ordered_json out;
    out["translation_units_added"] = result.summary.translation_units_added;
    out["translation_units_removed"] = result.summary.translation_units_removed;
    out["translation_units_changed"] = result.summary.translation_units_changed;
    out["include_hotspots_added"] = result.summary.include_hotspots_added;
    out["include_hotspots_removed"] = result.summary.include_hotspots_removed;
    out["include_hotspots_changed"] = result.summary.include_hotspots_changed;
    out["target_nodes_added"] = result.summary.target_nodes_added;
    out["target_nodes_removed"] = result.summary.target_nodes_removed;
    out["target_nodes_changed"] = result.summary.target_nodes_changed;
    out["target_edges_added"] = result.summary.target_edges_added;
    out["target_edges_removed"] = result.summary.target_edges_removed;
    out["target_edges_changed"] = result.summary.target_edges_changed;
    out["target_hubs_added"] = result.summary.target_hubs_added;
    out["target_hubs_removed"] = result.summary.target_hubs_removed;
    out["target_hubs_changed"] = result.summary.target_hubs_changed;
    return ordered_json(std::move(out));
}

ordered_json render_diffs(const CompareResult& result) {
    ordered_json out;
    out["translation_units"] =
        render_diff_group(result.translation_unit_diffs, render_translation_unit_diff);
    out["include_hotspots"] =
        render_diff_group(result.include_hotspot_diffs, render_include_hotspot_diff);
    out["target_nodes"] = render_diff_group(result.target_node_diffs, render_target_node_diff);
    out["target_edges"] = render_diff_group(result.target_edge_diffs, render_target_edge_diff);
    out["target_hubs"] = render_diff_group(result.target_hub_diffs, render_target_hub_diff);
    return ordered_json(std::move(out));
}

ordered_json render_configuration_drifts(const CompareResult& result) {
    auto out = ordered_json::array();
    for (const auto& drift : result.configuration_drifts) {
        ordered_json item;
        item["field"] = drift.field;
        item["baseline_value"] = render_scalar(drift.baseline_value);
        item["current_value"] = render_scalar(drift.current_value);
        item["severity"] = drift.severity;
        item["ci_policy_hint"] = drift.ci_policy_hint;
        out.push_back(std::move(item));
    }
    return ordered_json(std::move(out));
}

ordered_json render_data_availability_drifts(const CompareResult& result) {
    auto out = ordered_json::array();
    for (const auto& drift : result.data_availability_drifts) {
        ordered_json item;
        item["section"] = drift.section;
        item["baseline_state"] = drift.baseline_state;
        item["current_state"] = drift.current_state;
        out.push_back(std::move(item));
    }
    return ordered_json(std::move(out));
}

ordered_json render_project_identity_drift(const CompareResult& result) {
    if (!result.project_identity_drift.has_value()) return nullptr;
    const auto& drift = *result.project_identity_drift;
    ordered_json out;
    out["baseline_project_identity"] = drift.baseline_project_identity;
    out["current_project_identity"] = drift.current_project_identity;
    out["baseline_source_path_count"] = drift.baseline_source_path_count;
    out["current_source_path_count"] = drift.current_source_path_count;
    out["shared_source_path_count"] = drift.shared_source_path_count;
    return ordered_json(std::move(out));
}

ordered_json render_diagnostics(const CompareResult& result) {
    ordered_json out;
    out["configuration_drifts"] = render_configuration_drifts(result);
    out["data_availability_drifts"] = render_data_availability_drifts(result);
    out["project_identity_drift"] = render_project_identity_drift(result);
    return ordered_json(std::move(out));
}

}  // namespace

std::string JsonCompareAdapter::write_compare_report(const CompareResult& result) const {
    ordered_json document;
    document["format"] = "cmake-xray.compare";
    document["format_version"] = kCompareFormatVersion;
    document["report_type"] = "compare";
    document["inputs"] = render_inputs(result);
    document["summary"] = render_summary(result);
    document["diffs"] = render_diffs(result);
    document["diagnostics"] = render_diagnostics(result);
    return document.dump(2) + "\n";
}

}  // namespace xray::adapters::output
