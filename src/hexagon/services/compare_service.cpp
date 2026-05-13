#include "services/compare_service.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "hexagon/model/report_format_version.h"

namespace xray::hexagon::services {

namespace {

using model::AnalysisReportSnapshot;
using model::CompareDiagnosticConfigurationDrift;
using model::CompareDiffKind;
using model::CompareFieldChange;
using model::CompareHotspotAffectedUnitSnapshot;
using model::CompareIncludeHotspotSnapshot;
using model::CompareResult;
using model::CompareScalarArray;
using model::CompareScalarValue;
using model::CompareTargetEdgeSnapshot;
using model::CompareTargetSnapshot;
using model::CompareTranslationUnitSnapshot;
using model::ProjectIdentitySource;
using ports::driven::AnalysisReportReadResult;

constexpr char kSeparator = '\x1f';

std::string normalize_path(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    std::vector<std::string> segments;
    std::stringstream stream(value);
    std::string segment;
    while (std::getline(stream, segment, '/')) {
        if (!segment.empty() && segment != ".") segments.push_back(segment);
    }
    std::string normalized;
    for (const auto& part : segments) {
        if (!normalized.empty()) normalized += "/";
        normalized += part;
    }
    return normalized;
}

std::string join_key(const std::vector<std::string>& parts) {
    std::string key;
    for (const auto& part : parts) {
        if (!key.empty()) key += kSeparator;
        key += part;
    }
    return std::string(std::move(key));
}

CompareScalarValue scalar_string(std::string value) {
    CompareScalarValue out;
    out.value = std::move(value);
    return out;
}

CompareScalarValue scalar_int(int value) {
    CompareScalarValue out;
    out.value = static_cast<std::int64_t>(value);
    return out;
}

CompareScalarValue scalar_array(std::vector<std::string> values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    CompareScalarArray array;
    for (auto& value : values) array.push_back(scalar_string(std::move(value)));
    CompareScalarValue out;
    out.value = std::move(array);
    return out;
}

bool scalar_equal(const CompareScalarValue& lhs, const CompareScalarValue& rhs) {
    return lhs == rhs;
}

void add_change(std::vector<CompareFieldChange>& changes, std::string field,
                CompareScalarValue baseline, CompareScalarValue current) {
    if (scalar_equal(baseline, current)) return;
    changes.push_back({std::move(field), std::move(baseline), std::move(current)});
}

std::string source_text(ProjectIdentitySource source) {
    if (source == ProjectIdentitySource::cmake_file_api_source_root) {
        return "cmake_file_api_source_root";
    }
    return "fallback_compile_database_fingerprint";
}

CompareResult result_with_error(std::string code, std::string message) {
    CompareResult result;
    result.service_error = model::CompareServiceError{std::move(code), std::move(message)};
    return result;
}

CompareResult incompatible_format_versions(int baseline_version, int current_version) {
    return result_with_error(
        "incompatible_format_version",
        "compare: format_version combination (" + std::to_string(baseline_version) +
            ", " + std::to_string(current_version) +
            ") is not in the compatibility matrix");
}

CompareResult read_error(std::string side, const std::filesystem::path& path,
                         const AnalysisReportReadResult& read) {
    using ports::driven::AnalysisReportReadError;
    if (read.error == AnalysisReportReadError::file_not_accessible) {
        return result_with_error(side + "_read_failed",
                                 "compare: cannot read " + side + " file '" +
                                     path.string() + "'");
    }
    if (read.error == AnalysisReportReadError::invalid_json) {
        return result_with_error(side + "_read_failed",
                                 "compare: " + side + " is not valid JSON");
    }
    if (read.error == AnalysisReportReadError::schema_mismatch) {
        return result_with_error(side + "_read_failed",
                                 "compare: " + side +
                                     " does not match the analyze JSON schema");
    }
    if (read.error == AnalysisReportReadError::unsupported_report_type) {
        return result_with_error(side + "_read_failed",
                                 "compare: " + side +
                                     " has report_type='" + read.message +
                                     "'; only 'analyze' is supported");
    }
    if (read.error == AnalysisReportReadError::invalid_project_identity_source) {
        return result_with_error("invalid_project_identity_source",
                                 "compare: invalid project identity source '" +
                                     read.message + "'");
    }
    if (read.error == AnalysisReportReadError::unrecoverable_project_identity) {
        return result_with_error("unrecoverable_project_identity",
                                 "compare: project identity is empty or unrecoverable");
    }
    if (read.error == AnalysisReportReadError::incompatible_format_version) {
        return result_with_error(
            "incompatible_format_version",
            "compare: format_version combination (" +
                std::to_string(read.report.format_version) +
                ", unknown) is not in the compatibility matrix");
    }
    return result_with_error(side + "_read_failed", "compare: " + side + ": " + read.message);
}

bool format_versions_supported(const AnalysisReportSnapshot& baseline,
                               const AnalysisReportSnapshot& current) {
    return baseline.format_version == model::kReportFormatVersion &&
           current.format_version == model::kReportFormatVersion;
}

std::set<std::string> normalized_source_paths(
    const std::vector<CompareTranslationUnitSnapshot>& units) {
    std::set<std::string> paths;
    for (const auto& unit : units) paths.insert(normalize_path(unit.source_path));
    return std::set<std::string>(std::move(paths));
}

std::size_t shared_count(const std::set<std::string>& baseline,
                         const std::set<std::string>& current) {
    std::vector<std::string> shared;
    std::set_intersection(baseline.begin(), baseline.end(), current.begin(), current.end(),
                          std::back_inserter(shared));
    return shared.size();
}

void apply_inputs_and_identity(const AnalysisReportSnapshot& baseline,
                               const AnalysisReportSnapshot& current,
                               const ports::driving::CompareAnalysisRequest& request,
                               CompareResult& result) {
    result.inputs.baseline_path = baseline.path.empty() ? request.baseline_path.string() : baseline.path;
    result.inputs.current_path = current.path.empty() ? request.current_path.string() : current.path;
    result.inputs.baseline_format_version = baseline.format_version;
    result.inputs.current_format_version = current.format_version;
    result.inputs.project_identity = baseline.project_identity;
    result.inputs.project_identity_source = source_text(baseline.project_identity_source);
}

bool reject_incompatible_identity(const AnalysisReportSnapshot& baseline,
                                  const AnalysisReportSnapshot& current,
                                  const ports::driving::CompareAnalysisRequest& request,
                                  CompareResult& result) {
    if (baseline.project_identity_source != current.project_identity_source) {
        result.service_error = model::CompareServiceError{
            "project_identity_source_mismatch",
            "compare: project identity source mismatch (baseline=" +
                source_text(baseline.project_identity_source) + ", current=" +
                source_text(current.project_identity_source) + ")"};
        return true;
    }
    if (baseline.project_identity == current.project_identity) return false;
    if (baseline.project_identity_source !=
        ProjectIdentitySource::fallback_compile_database_fingerprint) {
        result.service_error = model::CompareServiceError{
            "project_identity_mismatch",
            "compare: project identity differs (baseline='" + baseline.project_identity +
                "', current='" + current.project_identity +
                "'); pass --allow-project-identity-drift to override (only valid for fallback_compile_database_fingerprint)"};
        return true;
    }
    if (!request.allow_project_identity_drift) {
        result.service_error = model::CompareServiceError{
            "project_identity_mismatch",
            "compare: project identity differs (baseline='" + baseline.project_identity +
                "', current='" + current.project_identity +
                "'); pass --allow-project-identity-drift to override (only valid for fallback_compile_database_fingerprint)"};
        return true;
    }
    result.inputs.project_identity = std::nullopt;
    const auto baseline_paths = normalized_source_paths(baseline.translation_units);
    const auto current_paths = normalized_source_paths(current.translation_units);
    result.project_identity_drift = model::CompareDiagnosticProjectIdentityDrift{
        baseline.project_identity,
        current.project_identity,
        baseline_paths.size(),
        current_paths.size(),
        shared_count(baseline_paths, current_paths)};
    return false;
}

std::vector<std::string> target_values(const std::vector<CompareTargetSnapshot>& targets) {
    std::vector<std::string> values;
    for (const auto& target : targets) {
        values.push_back(join_key({target.unique_key, target.display_name, target.type}));
    }
    return values;
}

std::vector<std::string> diagnostic_values(
    const std::vector<model::CompareDiagnosticSnapshot>& diagnostics) {
    std::vector<std::string> values;
    for (const auto& diagnostic : diagnostics) {
        values.push_back(join_key({diagnostic.severity, diagnostic.message}));
    }
    return values;
}

std::string tu_key(const CompareTranslationUnitSnapshot& unit) {
    return join_key({normalize_path(unit.source_path), normalize_path(unit.build_context_key)});
}

std::map<std::string, CompareTranslationUnitSnapshot> index_translation_units(
    const std::vector<CompareTranslationUnitSnapshot>& units) {
    std::map<std::string, CompareTranslationUnitSnapshot> indexed;
    for (const auto& unit : units) indexed.emplace(tu_key(unit), unit);
    return std::map<std::string, CompareTranslationUnitSnapshot>(std::move(indexed));
}

std::vector<CompareFieldChange> translation_unit_changes(
    const CompareTranslationUnitSnapshot& baseline,
    const CompareTranslationUnitSnapshot& current) {
    std::vector<CompareFieldChange> changes;
    add_change(changes, "metrics.arg_count", scalar_int(baseline.arg_count),
               scalar_int(current.arg_count));
    add_change(changes, "metrics.include_path_count",
               scalar_int(baseline.include_path_count),
               scalar_int(current.include_path_count));
    add_change(changes, "metrics.define_count", scalar_int(baseline.define_count),
               scalar_int(current.define_count));
    add_change(changes, "metrics.score", scalar_int(baseline.score),
               scalar_int(current.score));
    add_change(changes, "targets", scalar_array(target_values(baseline.targets)),
               scalar_array(target_values(current.targets)));
    add_change(changes, "diagnostics", scalar_array(diagnostic_values(baseline.diagnostics)),
               scalar_array(diagnostic_values(current.diagnostics)));
    return std::vector<CompareFieldChange>(std::move(changes));
}

void diff_translation_units(const AnalysisReportSnapshot& baseline,
                            const AnalysisReportSnapshot& current, CompareResult& result) {
    const auto baseline_units = index_translation_units(baseline.translation_units);
    const auto current_units = index_translation_units(current.translation_units);
    for (const auto& [key, unit] : current_units) {
        const auto found = baseline_units.find(key);
        if (found == baseline_units.end()) {
            result.translation_unit_diffs.push_back(
                {CompareDiffKind::added, unit.source_path, unit.build_context_key, {}});
            continue;
        }
        auto changes = translation_unit_changes(found->second, unit);
        if (!changes.empty()) {
            result.translation_unit_diffs.push_back(
                {CompareDiffKind::changed, unit.source_path, unit.build_context_key,
                 std::move(changes)});
        }
    }
    for (const auto& [key, unit] : baseline_units) {
        if (!current_units.contains(key)) {
            result.translation_unit_diffs.push_back(
                {CompareDiffKind::removed, unit.source_path, unit.build_context_key, {}});
        }
    }
}

std::string hotspot_key(const CompareIncludeHotspotSnapshot& hotspot) {
    return join_key({normalize_path(hotspot.header_path), hotspot.include_origin,
                     hotspot.include_depth_kind});
}

std::map<std::string, CompareIncludeHotspotSnapshot> index_hotspots(
    const std::vector<CompareIncludeHotspotSnapshot>& hotspots) {
    std::map<std::string, CompareIncludeHotspotSnapshot> indexed;
    for (const auto& hotspot : hotspots) indexed.emplace(hotspot_key(hotspot), hotspot);
    return std::map<std::string, CompareIncludeHotspotSnapshot>(std::move(indexed));
}

std::vector<std::string> affected_values(
    const std::vector<CompareHotspotAffectedUnitSnapshot>& units) {
    std::vector<std::string> values;
    for (const auto& unit : units) {
        values.push_back(join_key({normalize_path(unit.source_path),
                                   normalize_path(unit.build_context_key)}));
    }
    return values;
}

std::vector<CompareFieldChange> hotspot_changes(
    const CompareIncludeHotspotSnapshot& baseline,
    const CompareIncludeHotspotSnapshot& current) {
    std::vector<CompareFieldChange> changes;
    add_change(changes, "affected_total_count", scalar_int(baseline.affected_total_count),
               scalar_int(current.affected_total_count));
    add_change(changes, "affected_translation_units",
               scalar_array(affected_values(baseline.affected_translation_units)),
               scalar_array(affected_values(current.affected_translation_units)));
    add_change(changes, "diagnostics", scalar_array(diagnostic_values(baseline.diagnostics)),
               scalar_array(diagnostic_values(current.diagnostics)));
    return std::vector<CompareFieldChange>(std::move(changes));
}

void diff_hotspots(const AnalysisReportSnapshot& baseline,
                   const AnalysisReportSnapshot& current, CompareResult& result) {
    const auto baseline_hotspots = index_hotspots(baseline.include_hotspots);
    const auto current_hotspots = index_hotspots(current.include_hotspots);
    for (const auto& [key, hotspot] : current_hotspots) {
        const auto found = baseline_hotspots.find(key);
        if (found == baseline_hotspots.end()) {
            result.include_hotspot_diffs.push_back(
                {CompareDiffKind::added, hotspot.header_path, hotspot.include_origin,
                 hotspot.include_depth_kind, {}});
            continue;
        }
        auto changes = hotspot_changes(found->second, hotspot);
        if (!changes.empty()) {
            result.include_hotspot_diffs.push_back(
                {CompareDiffKind::changed, hotspot.header_path, hotspot.include_origin,
                 hotspot.include_depth_kind, std::move(changes)});
        }
    }
    for (const auto& [key, hotspot] : baseline_hotspots) {
        if (!current_hotspots.contains(key)) {
            result.include_hotspot_diffs.push_back(
                {CompareDiffKind::removed, hotspot.header_path, hotspot.include_origin,
                 hotspot.include_depth_kind, {}});
        }
    }
}

std::map<std::string, CompareTargetSnapshot> index_nodes(
    const std::vector<CompareTargetSnapshot>& nodes) {
    std::map<std::string, CompareTargetSnapshot> indexed;
    for (const auto& node : nodes) indexed.emplace(node.unique_key, node);
    return std::map<std::string, CompareTargetSnapshot>(std::move(indexed));
}

std::vector<CompareFieldChange> node_changes(const CompareTargetSnapshot& baseline,
                                             const CompareTargetSnapshot& current) {
    std::vector<CompareFieldChange> changes;
    add_change(changes, "display_name", scalar_string(baseline.display_name),
               scalar_string(current.display_name));
    add_change(changes, "type", scalar_string(baseline.type), scalar_string(current.type));
    return std::vector<CompareFieldChange>(std::move(changes));
}

void diff_nodes(const AnalysisReportSnapshot& baseline,
                const AnalysisReportSnapshot& current, CompareResult& result) {
    const auto baseline_nodes = index_nodes(baseline.target_nodes);
    const auto current_nodes = index_nodes(current.target_nodes);
    for (const auto& [key, node] : current_nodes) {
        const auto found = baseline_nodes.find(key);
        if (found == baseline_nodes.end()) {
            result.target_node_diffs.push_back({CompareDiffKind::added, key, {}});
            continue;
        }
        auto changes = node_changes(found->second, node);
        if (!changes.empty()) {
            result.target_node_diffs.push_back({CompareDiffKind::changed, key,
                                                std::move(changes)});
        }
    }
    for (const auto& [key, node] : baseline_nodes) {
        if (!current_nodes.contains(key)) result.target_node_diffs.push_back(
            {CompareDiffKind::removed, node.unique_key, {}});
    }
}

std::string edge_key(const CompareTargetEdgeSnapshot& edge) {
    return join_key({edge.from_unique_key, edge.to_unique_key, edge.kind});
}

std::map<std::string, CompareTargetEdgeSnapshot> index_edges(
    const std::vector<CompareTargetEdgeSnapshot>& edges) {
    std::map<std::string, CompareTargetEdgeSnapshot> indexed;
    for (const auto& edge : edges) indexed.emplace(edge_key(edge), edge);
    return std::map<std::string, CompareTargetEdgeSnapshot>(std::move(indexed));
}

std::vector<CompareFieldChange> edge_changes(const CompareTargetEdgeSnapshot& baseline,
                                             const CompareTargetEdgeSnapshot& current) {
    std::vector<CompareFieldChange> changes;
    add_change(changes, "resolution", scalar_string(baseline.resolution),
               scalar_string(current.resolution));
    add_change(changes, "from_display_name", scalar_string(baseline.from_display_name),
               scalar_string(current.from_display_name));
    add_change(changes, "to_display_name", scalar_string(baseline.to_display_name),
               scalar_string(current.to_display_name));
    return std::vector<CompareFieldChange>(std::move(changes));
}

void diff_edges(const AnalysisReportSnapshot& baseline,
                const AnalysisReportSnapshot& current, CompareResult& result) {
    const auto baseline_edges = index_edges(baseline.target_edges);
    const auto current_edges = index_edges(current.target_edges);
    for (const auto& [key, edge] : current_edges) {
        const auto found = baseline_edges.find(key);
        if (found == baseline_edges.end()) {
            result.target_edge_diffs.push_back(
                {CompareDiffKind::added, edge.from_unique_key, edge.to_unique_key,
                 edge.kind, {}});
            continue;
        }
        auto changes = edge_changes(found->second, edge);
        if (!changes.empty()) {
            result.target_edge_diffs.push_back(
                {CompareDiffKind::changed, edge.from_unique_key, edge.to_unique_key,
                 edge.kind, std::move(changes)});
        }
    }
    for (const auto& [key, edge] : baseline_edges) {
        if (!current_edges.contains(key)) {
            result.target_edge_diffs.push_back(
                {CompareDiffKind::removed, edge.from_unique_key, edge.to_unique_key,
                 edge.kind, {}});
        }
    }
}

std::map<std::string, CompareTargetSnapshot> index_hubs(
    const std::vector<CompareTargetSnapshot>& hubs, const std::string& direction) {
    std::map<std::string, CompareTargetSnapshot> indexed;
    for (const auto& hub : hubs) indexed.emplace(join_key({hub.unique_key, direction}), hub);
    return indexed;
}

void append_hub_diffs(const std::map<std::string, CompareTargetSnapshot>& baseline,
                      const std::map<std::string, CompareTargetSnapshot>& current,
                      const std::string& direction, CompareResult& result) {
    for (const auto& [key, hub] : current) {
        if (!baseline.contains(key)) {
            result.target_hub_diffs.push_back(
                {CompareDiffKind::added, hub.unique_key, direction});
        }
    }
    for (const auto& [key, hub] : baseline) {
        if (!current.contains(key)) {
            result.target_hub_diffs.push_back(
                {CompareDiffKind::removed, hub.unique_key, direction});
        }
    }
}

void diff_hubs(const AnalysisReportSnapshot& baseline,
               const AnalysisReportSnapshot& current, CompareResult& result) {
    append_hub_diffs(index_hubs(baseline.target_hubs_in, "inbound"),
                     index_hubs(current.target_hubs_in, "inbound"), "inbound", result);
    append_hub_diffs(index_hubs(baseline.target_hubs_out, "outbound"),
                     index_hubs(current.target_hubs_out, "outbound"), "outbound", result);
}

void add_configuration_drift(std::vector<CompareDiagnosticConfigurationDrift>& drifts,
                             std::string field, CompareScalarValue baseline,
                             CompareScalarValue current) {
    if (!scalar_equal(baseline, current)) {
        drifts.push_back({std::move(field), std::move(baseline), std::move(current)});
    }
}

void collect_configuration_drifts(const AnalysisReportSnapshot& baseline,
                                  const AnalysisReportSnapshot& current,
                                  CompareResult& result) {
    add_configuration_drift(result.configuration_drifts,
                            "analysis_configuration.analysis_sections",
                            scalar_array(baseline.configuration.analysis_sections),
                            scalar_array(current.configuration.analysis_sections));
    add_configuration_drift(result.configuration_drifts,
                            "analysis_configuration.tu_thresholds.arg_count",
                            scalar_int(baseline.configuration.tu_threshold_arg_count),
                            scalar_int(current.configuration.tu_threshold_arg_count));
    add_configuration_drift(result.configuration_drifts,
                            "analysis_configuration.tu_thresholds.include_path_count",
                            scalar_int(baseline.configuration.tu_threshold_include_path_count),
                            scalar_int(current.configuration.tu_threshold_include_path_count));
    add_configuration_drift(result.configuration_drifts,
                            "analysis_configuration.tu_thresholds.define_count",
                            scalar_int(baseline.configuration.tu_threshold_define_count),
                            scalar_int(current.configuration.tu_threshold_define_count));
    add_configuration_drift(result.configuration_drifts,
                            "analysis_configuration.min_hotspot_tus",
                            scalar_int(baseline.configuration.min_hotspot_tus),
                            scalar_int(current.configuration.min_hotspot_tus));
    add_configuration_drift(result.configuration_drifts,
                            "analysis_configuration.target_hub_in_threshold",
                            scalar_int(baseline.configuration.target_hub_in_threshold),
                            scalar_int(current.configuration.target_hub_in_threshold));
    add_configuration_drift(result.configuration_drifts,
                            "analysis_configuration.target_hub_out_threshold",
                            scalar_int(baseline.configuration.target_hub_out_threshold),
                            scalar_int(current.configuration.target_hub_out_threshold));
    add_configuration_drift(result.configuration_drifts, "include_filter.include_scope",
                            scalar_string(baseline.configuration.include_scope),
                            scalar_string(current.configuration.include_scope));
    add_configuration_drift(result.configuration_drifts, "include_filter.include_depth",
                            scalar_string(baseline.configuration.include_depth),
                            scalar_string(current.configuration.include_depth));
}

bool section_state_differs(std::string section, std::string baseline_state,
                           std::string current_state, CompareResult& result) {
    if (baseline_state == current_state) return false;
    result.data_availability_drifts.push_back(
        {std::move(section), std::move(baseline_state), std::move(current_state)});
    return true;
}

template <typename Diff>
int count_kind(const std::vector<Diff>& diffs, CompareDiffKind kind) {
    return static_cast<int>(std::count_if(
        diffs.begin(), diffs.end(), [kind](const Diff& diff) { return diff.kind == kind; }));
}

void collect_summary(CompareResult& result) {
    result.summary.translation_units_added =
        count_kind(result.translation_unit_diffs, CompareDiffKind::added);
    result.summary.translation_units_removed =
        count_kind(result.translation_unit_diffs, CompareDiffKind::removed);
    result.summary.translation_units_changed =
        count_kind(result.translation_unit_diffs, CompareDiffKind::changed);
    result.summary.include_hotspots_added =
        count_kind(result.include_hotspot_diffs, CompareDiffKind::added);
    result.summary.include_hotspots_removed =
        count_kind(result.include_hotspot_diffs, CompareDiffKind::removed);
    result.summary.include_hotspots_changed =
        count_kind(result.include_hotspot_diffs, CompareDiffKind::changed);
    result.summary.target_nodes_added =
        count_kind(result.target_node_diffs, CompareDiffKind::added);
    result.summary.target_nodes_removed =
        count_kind(result.target_node_diffs, CompareDiffKind::removed);
    result.summary.target_nodes_changed =
        count_kind(result.target_node_diffs, CompareDiffKind::changed);
    result.summary.target_edges_added =
        count_kind(result.target_edge_diffs, CompareDiffKind::added);
    result.summary.target_edges_removed =
        count_kind(result.target_edge_diffs, CompareDiffKind::removed);
    result.summary.target_edges_changed =
        count_kind(result.target_edge_diffs, CompareDiffKind::changed);
    result.summary.target_hubs_added =
        count_kind(result.target_hub_diffs, CompareDiffKind::added);
    result.summary.target_hubs_removed =
        count_kind(result.target_hub_diffs, CompareDiffKind::removed);
}

}  // namespace

CompareService::CompareService(const ports::driven::AnalysisReportReaderPort& reader)
    : reader_(reader) {}

CompareResult CompareService::compare(ports::driving::CompareAnalysisRequest request) const {
    const auto baseline = reader_.read_analysis_report(request.baseline_path);
    if (baseline.error == ports::driven::AnalysisReportReadError::incompatible_format_version) {
        const auto current = reader_.read_analysis_report(request.current_path);
        if (current.is_success() ||
            current.error ==
                ports::driven::AnalysisReportReadError::incompatible_format_version) {
            return incompatible_format_versions(baseline.report.format_version,
                                                current.report.format_version);
        }
        return read_error("baseline", request.baseline_path, baseline);
    }
    if (!baseline.is_success()) return read_error("baseline", request.baseline_path, baseline);
    const auto current = reader_.read_analysis_report(request.current_path);
    if (current.error == ports::driven::AnalysisReportReadError::incompatible_format_version) {
        return incompatible_format_versions(baseline.report.format_version,
                                            current.report.format_version);
    }
    if (!current.is_success()) return read_error("current", request.current_path, current);
    if (!format_versions_supported(baseline.report, current.report)) {
        return incompatible_format_versions(baseline.report.format_version,
                                            current.report.format_version);
    }

    CompareResult result;
    apply_inputs_and_identity(baseline.report, current.report, request, result);
    if (reject_incompatible_identity(baseline.report, current.report, request, result)) {
        return result;
    }

    collect_configuration_drifts(baseline.report, current.report, result);
    diff_translation_units(baseline.report, current.report, result);
    diff_hotspots(baseline.report, current.report, result);
    const bool target_graph_drift = section_state_differs(
        "target_graph", baseline.report.target_graph_state, current.report.target_graph_state,
        result);
    const bool target_hubs_drift = section_state_differs(
        "target_hubs", baseline.report.target_hubs_state, current.report.target_hubs_state,
        result);
    if (!target_graph_drift) {
        diff_nodes(baseline.report, current.report, result);
        diff_edges(baseline.report, current.report, result);
    }
    if (!target_hubs_drift) diff_hubs(baseline.report, current.report, result);
    collect_summary(result);
    return result;
}

}  // namespace xray::hexagon::services
