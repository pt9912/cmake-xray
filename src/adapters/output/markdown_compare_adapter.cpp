#include "adapters/output/markdown_compare_adapter.h"

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace xray::adapters::output {

namespace {

using xray::hexagon::model::CompareDiffKind;
using xray::hexagon::model::CompareFieldChange;
using xray::hexagon::model::CompareResult;
using xray::hexagon::model::CompareScalarArray;
using xray::hexagon::model::CompareScalarValue;

std::string scalar_text(const CompareScalarValue& value);

std::string scalar_array_text(const CompareScalarArray& values) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out << ", ";
        out << scalar_text(values[i]);
    }
    return out.str();
}

std::string scalar_text(const CompareScalarValue& value) {
    if (std::holds_alternative<std::string>(value.value)) {
        return std::get<std::string>(value.value);
    }
    if (std::holds_alternative<std::int64_t>(value.value)) {
        return std::to_string(std::get<std::int64_t>(value.value));
    }
    if (std::holds_alternative<bool>(value.value)) {
        return std::get<bool>(value.value) ? "true" : "false";
    }
    if (std::holds_alternative<CompareScalarArray>(value.value)) {
        return scalar_array_text(std::get<CompareScalarArray>(value.value));
    }
    return "null";
}

std::string code(std::string_view value) {
    std::size_t longest_backtick_run = 0;
    std::size_t current_backtick_run = 0;
    for (const char ch : value) {
        if (ch == '`') {
            ++current_backtick_run;
            longest_backtick_run = std::max(longest_backtick_run, current_backtick_run);
        } else {
            current_backtick_run = 0;
        }
    }
    const auto delimiter = std::string(longest_backtick_run + 1, '`');
    std::ostringstream out;
    out << delimiter;
    if (longest_backtick_run != 0) out << ' ';
    for (const char ch : value) {
        if (ch == '|') {
            out << "\\|";
        } else {
            out << ch;
        }
    }
    if (longest_backtick_run != 0) out << ' ';
    out << delimiter;
    return out.str();
}

void render_changes(std::ostringstream& out,
                    const std::vector<CompareFieldChange>& changes) {
    if (changes.empty()) return;
    out << "\n| Field | Baseline | Current |\n";
    out << "| --- | --- | --- |\n";
    for (const auto& change : changes) {
        out << "| " << code(change.field) << " | "
            << code(scalar_text(change.baseline_value)) << " | "
            << code(scalar_text(change.current_value)) << " |\n";
    }
}

bool has_kind(const auto& diffs, CompareDiffKind kind) {
    for (const auto& diff : diffs) {
        if (diff.kind == kind) return true;
    }
    return false;
}

void render_translation_units(std::ostringstream& out, const CompareResult& result,
                              CompareDiffKind kind, std::string_view title) {
    if (!has_kind(result.translation_unit_diffs, kind)) return;
    out << "\n### Translation Units (" << title << ")\n\n";
    for (const auto& diff : result.translation_unit_diffs) {
        if (diff.kind != kind) continue;
        out << "- " << code(diff.source_path) << " [" << code(diff.build_context_key)
            << "]\n";
        render_changes(out, diff.changes);
    }
}

void render_include_hotspots(std::ostringstream& out, const CompareResult& result,
                             CompareDiffKind kind, std::string_view title) {
    if (!has_kind(result.include_hotspot_diffs, kind)) return;
    out << "\n### Include Hotspots (" << title << ")\n\n";
    for (const auto& diff : result.include_hotspot_diffs) {
        if (diff.kind != kind) continue;
        out << "- " << code(diff.header_path) << " [" << code(diff.include_origin)
            << ", " << code(diff.include_depth_kind) << "]\n";
        render_changes(out, diff.changes);
    }
}

void render_target_nodes(std::ostringstream& out, const CompareResult& result,
                         CompareDiffKind kind, std::string_view title) {
    if (!has_kind(result.target_node_diffs, kind)) return;
    out << "\n### Target Nodes (" << title << ")\n\n";
    for (const auto& diff : result.target_node_diffs) {
        if (diff.kind != kind) continue;
        out << "- " << code(diff.unique_key) << "\n";
        render_changes(out, diff.changes);
    }
}

void render_target_edges(std::ostringstream& out, const CompareResult& result,
                         CompareDiffKind kind, std::string_view title) {
    if (!has_kind(result.target_edge_diffs, kind)) return;
    out << "\n### Target Edges (" << title << ")\n\n";
    for (const auto& diff : result.target_edge_diffs) {
        if (diff.kind != kind) continue;
        out << "- " << code(diff.from_unique_key) << " -> "
            << code(diff.to_unique_key) << " [" << code(diff.kind_text) << "]\n";
        render_changes(out, diff.changes);
    }
}

void render_target_hubs(std::ostringstream& out, const CompareResult& result,
                        CompareDiffKind kind, std::string_view title) {
    if (!has_kind(result.target_hub_diffs, kind)) return;
    out << "\n### Target Hubs (" << title << ")\n\n";
    for (const auto& diff : result.target_hub_diffs) {
        if (diff.kind != kind) continue;
        out << "- " << code(diff.unique_key) << " [" << code(diff.direction) << "]\n";
    }
}

void render_summary(std::ostringstream& out, const CompareResult& result) {
    out << "## Summary\n\n";
    out << "| Group | Added | Removed | Changed |\n";
    out << "| --- | ---: | ---: | ---: |\n";
    out << "| Translation units | " << result.summary.translation_units_added << " | "
        << result.summary.translation_units_removed << " | "
        << result.summary.translation_units_changed << " |\n";
    out << "| Include hotspots | " << result.summary.include_hotspots_added << " | "
        << result.summary.include_hotspots_removed << " | "
        << result.summary.include_hotspots_changed << " |\n";
    out << "| Target nodes | " << result.summary.target_nodes_added << " | "
        << result.summary.target_nodes_removed << " | "
        << result.summary.target_nodes_changed << " |\n";
    out << "| Target edges | " << result.summary.target_edges_added << " | "
        << result.summary.target_edges_removed << " | "
        << result.summary.target_edges_changed << " |\n";
    out << "| Target hubs | " << result.summary.target_hubs_added << " | "
        << result.summary.target_hubs_removed << " | "
        << result.summary.target_hubs_changed << " |\n";
}

void render_diagnostics(std::ostringstream& out, const CompareResult& result) {
    if (result.configuration_drifts.empty() &&
        result.data_availability_drifts.empty() &&
        !result.project_identity_drift.has_value()) {
        return;
    }
    out << "\n## Diagnostics\n\n";
    for (const auto& drift : result.configuration_drifts) {
        out << "- configuration_drift: " << code(drift.field) << "\n";
        out << "  - baseline=" << code(scalar_text(drift.baseline_value))
            << ", current=" << code(scalar_text(drift.current_value)) << "\n";
        out << "  - severity=" << code(drift.severity)
            << ", ci_policy_hint=" << code(drift.ci_policy_hint) << "\n";
    }
    for (const auto& drift : result.data_availability_drifts) {
        out << "- data_availability_drift: " << code(drift.section) << "\n";
        out << "  - baseline=" << code(drift.baseline_state)
            << ", current=" << code(drift.current_state) << "\n";
    }
    if (result.project_identity_drift.has_value()) {
        const auto& drift = *result.project_identity_drift;
        out << "- project_identity_drift\n";
        out << "  - baseline=" << code(drift.baseline_project_identity)
            << ", current=" << code(drift.current_project_identity) << "\n";
        out << "  - baseline_source_paths=" << drift.baseline_source_path_count
            << ", current_source_paths=" << drift.current_source_path_count
            << ", shared_source_paths=" << drift.shared_source_path_count << "\n";
    }
}

}  // namespace

std::string MarkdownCompareAdapter::write_compare_report(const CompareResult& result) const {
    std::ostringstream out;
    out << "# Compare Report\n\n";
    out << "- Baseline: " << code(result.inputs.baseline_path) << " (v"
        << result.inputs.baseline_format_version << ")\n";
    out << "- Current: " << code(result.inputs.current_path) << " (v"
        << result.inputs.current_format_version << ")\n";
    if (result.inputs.project_identity.has_value()) {
        out << "- Project identity: " << code(*result.inputs.project_identity) << "\n";
    } else {
        out << "- Project identity: drift allowed\n";
    }
    out << "- Project identity source: " << code(result.inputs.project_identity_source)
        << "\n\n";
    render_summary(out, result);
    render_translation_units(out, result, CompareDiffKind::added, "added");
    render_translation_units(out, result, CompareDiffKind::removed, "removed");
    render_translation_units(out, result, CompareDiffKind::changed, "changed");
    render_include_hotspots(out, result, CompareDiffKind::added, "added");
    render_include_hotspots(out, result, CompareDiffKind::removed, "removed");
    render_include_hotspots(out, result, CompareDiffKind::changed, "changed");
    render_target_nodes(out, result, CompareDiffKind::added, "added");
    render_target_nodes(out, result, CompareDiffKind::removed, "removed");
    render_target_nodes(out, result, CompareDiffKind::changed, "changed");
    render_target_edges(out, result, CompareDiffKind::added, "added");
    render_target_edges(out, result, CompareDiffKind::removed, "removed");
    render_target_edges(out, result, CompareDiffKind::changed, "changed");
    render_target_hubs(out, result, CompareDiffKind::added, "added");
    render_target_hubs(out, result, CompareDiffKind::removed, "removed");
    render_diagnostics(out, result);
    return out.str();
}

}  // namespace xray::adapters::output
