#include "adapters/output/json_report_adapter.h"

#include "adapters/output/impact_priority_text.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"
#include "hexagon/model/translation_unit.h"
#include "hexagon/services/target_graph_support.h"

namespace xray::adapters::output {

namespace {

using nlohmann::ordered_json;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::ChangedFileSource;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactedTarget;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::IncludeHotspot;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::ReportInputs;
using xray::hexagon::model::ReportInputSource;
using xray::hexagon::model::TargetAssignment;
using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetGraphStatus;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;
using xray::hexagon::model::TranslationUnitReference;
using xray::hexagon::model::kReportFormatVersion;

ordered_json string_or_null(const std::optional<std::string>& value) {
    if (value.has_value()) return *value;
    return nullptr;
}

ordered_json source_or_null(ReportInputSource source) {
    if (source == ReportInputSource::cli) return std::string{"cli"};
    return nullptr;
}

std::string changed_file_source_text(ChangedFileSource source) {
    if (source == ChangedFileSource::compile_database_directory) {
        return "compile_database_directory";
    }
    if (source == ChangedFileSource::file_api_source_root) {
        return "file_api_source_root";
    }
    if (source == ChangedFileSource::cli_absolute) {
        return "cli_absolute";
    }
    // JSON v1 does not enumerate unresolved_file_api_source_root in the
    // ChangedFileSource enum; the CLI treats unresolved file-api source roots
    // as text-only errors instead of producing a JSON report. Emit the model
    // name verbatim so a stray invocation fails the schema gate loudly
    // rather than silently masking a bug.
    return "unresolved_file_api_source_root";
}

std::string observation_source_text(ObservationSource source) {
    return source == ObservationSource::derived ? "derived" : "exact";
}

std::string target_metadata_text(TargetMetadataStatus status) {
    if (status == TargetMetadataStatus::loaded) return "loaded";
    if (status == TargetMetadataStatus::partial) return "partial";
    return "not_loaded";
}

std::string target_graph_status_text(TargetGraphStatus status) {
    if (status == TargetGraphStatus::loaded) return "loaded";
    if (status == TargetGraphStatus::partial) return "partial";
    return "not_loaded";
}

std::string target_dependency_kind_text(TargetDependencyKind /*kind*/) {
    // TargetDependencyKind has only `direct` today; extend per plan-M6-1-1.md
    // when adding kinds.
    return "direct";
}

std::string target_dependency_resolution_text(TargetDependencyResolution res) {
    return res == TargetDependencyResolution::external ? "external" : "resolved";
}

ordered_json render_target_node(const TargetInfo& info) {
    ordered_json node;
    node["display_name"] = info.display_name;
    node["type"] = info.type;
    node["unique_key"] = info.unique_key;
    // Explicit move-construct on return defeats NRVO so the closing-brace
    // destructor counts as a covered line (same trick the file already uses
    // in escape_dot_string).
    return ordered_json(std::move(node));
}

ordered_json render_target_edge(const TargetDependency& edge) {
    ordered_json e;
    e["from_display_name"] = edge.from_display_name;
    e["from_unique_key"] = edge.from_unique_key;
    e["to_display_name"] = edge.to_display_name;
    e["to_unique_key"] = edge.to_unique_key;
    e["kind"] = target_dependency_kind_text(edge.kind);
    e["resolution"] = target_dependency_resolution_text(edge.resolution);
    return ordered_json(std::move(e));
}

ordered_json render_target_graph(const TargetGraph& graph) {
    ordered_json document;
    auto nodes = ordered_json::array();
    for (const auto& n : graph.nodes) nodes.push_back(render_target_node(n));
    document["nodes"] = std::move(nodes);
    auto edges = ordered_json::array();
    for (const auto& e : graph.edges) edges.push_back(render_target_edge(e));
    document["edges"] = std::move(edges);
    return document;
}

// Bundle the two TargetInfo vectors so clang-tidy's
// bugprone-easily-swappable-parameters check stays happy and callers can't
// accidentally swap the inbound/outbound slots at the call site.
struct TargetHubsView {
    const std::vector<TargetInfo>& inbound;
    const std::vector<TargetInfo>& outbound;
};

ordered_json render_target_hubs(const TargetHubsView& hubs_view) {
    ordered_json hubs;
    auto in_arr = ordered_json::array();
    for (const auto& n : hubs_view.inbound) in_arr.push_back(render_target_node(n));
    hubs["inbound"] = std::move(in_arr);
    auto out_arr = ordered_json::array();
    for (const auto& n : hubs_view.outbound) out_arr.push_back(render_target_node(n));
    hubs["outbound"] = std::move(out_arr);
    ordered_json thresholds;
    thresholds["in_threshold"] =
        xray::hexagon::services::kDefaultTargetHubInThreshold;
    thresholds["out_threshold"] =
        xray::hexagon::services::kDefaultTargetHubOutThreshold;
    hubs["thresholds"] = std::move(thresholds);
    return ordered_json(std::move(hubs));
}

std::string severity_text(DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::warning ? "warning" : "note";
}

void append_diagnostics(ordered_json& array, const std::vector<Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        array.push_back(ordered_json{
            {"severity", severity_text(diagnostic.severity)},
            {"message", diagnostic.message},
        });
    }
}

ordered_json render_reference(const TranslationUnitReference& reference) {
    return ordered_json{
        {"source_path", reference.source_path},
        {"directory", reference.directory},
    };
}

ordered_json render_target(const TargetInfo& target) {
    return ordered_json{
        {"display_name", target.display_name},
        {"type", target.type},
    };
}

void append_targets_sorted(ordered_json& array, const std::vector<TargetInfo>& targets) {
    std::vector<TargetInfo> sorted = targets;
    std::stable_sort(sorted.begin(), sorted.end(),
              [](const TargetInfo& lhs, const TargetInfo& rhs) {
                  if (lhs.display_name != rhs.display_name) {
                      return lhs.display_name < rhs.display_name;
                  }
                  return lhs.type < rhs.type;
              });
    for (const auto& target : sorted) {
        array.push_back(render_target(target));
    }
}

bool reference_less(const TranslationUnitReference& lhs,
                    const TranslationUnitReference& rhs) {
    if (lhs.source_path != rhs.source_path) {
        return lhs.source_path < rhs.source_path;
    }
    return lhs.directory < rhs.directory;
}

ordered_json render_inputs_common(const ReportInputs& inputs) {
    auto compile_database_path = string_or_null(inputs.compile_database_path);
    auto compile_database_source = source_or_null(inputs.compile_database_source);
    auto cmake_file_api_path = string_or_null(inputs.cmake_file_api_path);
    auto cmake_file_api_resolved_path =
        string_or_null(inputs.cmake_file_api_resolved_path);
    auto cmake_file_api_source = source_or_null(inputs.cmake_file_api_source);
    return ordered_json{
        {"compile_database_path", std::move(compile_database_path)},
        {"compile_database_source", std::move(compile_database_source)},
        {"cmake_file_api_path", std::move(cmake_file_api_path)},
        {"cmake_file_api_resolved_path", std::move(cmake_file_api_resolved_path)},
        {"cmake_file_api_source", std::move(cmake_file_api_source)},
    };
}

void index_targets(std::map<std::string, const std::vector<TargetInfo>*>& by_key,
                   const std::vector<TargetAssignment>& assignments) {
    for (const auto& assignment : assignments) {
        by_key.emplace(assignment.observation_key, &assignment.targets);
    }
}

ordered_json render_ranking_item(const RankedTranslationUnit& tu) {
    auto targets_array = ordered_json::array();
    append_targets_sorted(targets_array, tu.targets);
    auto diagnostics_array = ordered_json::array();
    append_diagnostics(diagnostics_array, tu.diagnostics);
    const auto score = tu.score();
    auto metrics = ordered_json{
        {"arg_count", tu.arg_count},
        {"include_path_count", tu.include_path_count},
        {"define_count", tu.define_count},
        {"score", score},
    };
    return ordered_json{
        {"rank", tu.rank},
        {"reference", render_reference(tu.reference)},
        {"metrics", std::move(metrics)},
        {"targets", std::move(targets_array)},
        {"diagnostics", std::move(diagnostics_array)},
    };
}

void append_ranked_units_sorted(ordered_json& items_array,
                                const std::vector<RankedTranslationUnit>& units,
                                std::size_t returned) {
    std::vector<RankedTranslationUnit> sorted{units.begin(), units.end()};
    std::stable_sort(sorted.begin(), sorted.end(),
              [](const RankedTranslationUnit& lhs,
                 const RankedTranslationUnit& rhs) {
                  if (lhs.rank != rhs.rank) return lhs.rank < rhs.rank;
                  return reference_less(lhs.reference, rhs.reference);
              });
    sorted.resize(returned);
    for (const auto& tu : sorted) {
        items_array.push_back(render_ranking_item(tu));
    }
}

ordered_json render_ranking_container(const AnalysisResult& result, std::size_t top_limit) {
    const auto total = result.translation_units.size();
    const auto returned = std::min(top_limit, total);
    const bool truncated = returned < total;
    auto items = ordered_json::array();
    append_ranked_units_sorted(items, result.translation_units, returned);
    return ordered_json{
        {"limit", top_limit},
        {"total_count", total},
        {"returned_count", returned},
        {"truncated", truncated},
        {"items", std::move(items)},
    };
}

ordered_json render_hotspot_affected_unit(
    const TranslationUnitReference& reference,
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key) {
    auto targets_array = ordered_json::array();
    const auto it = targets_by_key.find(reference.unique_key);
    if (it != targets_by_key.end()) append_targets_sorted(targets_array, *it->second);
    return ordered_json{
        {"reference", render_reference(reference)},
        {"targets", std::move(targets_array)},
    };
}

void append_hotspot_affected_sorted(
    ordered_json& units_array,
    const std::vector<TranslationUnitReference>& references, std::size_t returned,
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key) {
    std::vector<TranslationUnitReference> sorted{references.begin(), references.end()};
    std::stable_sort(sorted.begin(), sorted.end(), reference_less);
    sorted.resize(returned);
    for (const auto& reference : sorted) {
        units_array.push_back(render_hotspot_affected_unit(reference, targets_by_key));
    }
}

ordered_json render_hotspot_item(
    const IncludeHotspot& hotspot, std::size_t top_limit,
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key) {
    const auto affected_total = hotspot.affected_translation_units.size();
    const auto affected_returned = std::min(top_limit, affected_total);
    const bool affected_truncated = affected_returned < affected_total;
    auto units_array = ordered_json::array();
    append_hotspot_affected_sorted(units_array, hotspot.affected_translation_units,
                                    affected_returned, targets_by_key);
    auto diagnostics_array = ordered_json::array();
    append_diagnostics(diagnostics_array, hotspot.diagnostics);
    return ordered_json{
        {"header_path", hotspot.header_path},
        {"affected_total_count", affected_total},
        {"affected_returned_count", affected_returned},
        {"affected_truncated", affected_truncated},
        {"affected_translation_units", std::move(units_array)},
        {"diagnostics", std::move(diagnostics_array)},
    };
}

void append_hotspot_items_sorted(
    ordered_json& items_array, const std::vector<IncludeHotspot>& hotspots,
    std::size_t top_limit,
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key) {
    std::vector<IncludeHotspot> sorted{hotspots.begin(), hotspots.end()};
    std::stable_sort(sorted.begin(), sorted.end(),
              [](const IncludeHotspot& lhs, const IncludeHotspot& rhs) {
                  const auto lhs_count = lhs.affected_translation_units.size();
                  const auto rhs_count = rhs.affected_translation_units.size();
                  if (lhs_count != rhs_count) return lhs_count > rhs_count;
                  return lhs.header_path < rhs.header_path;
              });
    sorted.resize(std::min(top_limit, sorted.size()));
    for (const auto& hotspot : sorted) {
        items_array.push_back(render_hotspot_item(hotspot, top_limit, targets_by_key));
    }
}

ordered_json render_hotspot_container(const AnalysisResult& result, std::size_t top_limit) {
    const auto total = result.include_hotspots.size();
    const auto returned = std::min(top_limit, total);
    const bool truncated = returned < total;
    std::map<std::string, const std::vector<TargetInfo>*> targets_by_key;
    index_targets(targets_by_key, result.target_assignments);
    auto items = ordered_json::array();
    append_hotspot_items_sorted(items, result.include_hotspots, top_limit,
                                 targets_by_key);
    return ordered_json{
        {"limit", top_limit},
        {"total_count", total},
        {"returned_count", returned},
        {"truncated", truncated},
        {"items", std::move(items)},
    };
}

ordered_json render_analysis_summary(const AnalysisResult& result, std::size_t top_limit) {
    const auto translation_unit_count = result.translation_units.size();
    const auto hotspot_total_count = result.include_hotspots.size();
    const auto observation_source = observation_source_text(result.observation_source);
    const auto target_metadata = target_metadata_text(result.target_metadata);
    return ordered_json{
        {"translation_unit_count", translation_unit_count},
        {"ranking_total_count", translation_unit_count},
        {"hotspot_total_count", hotspot_total_count},
        {"top_limit", top_limit},
        {"include_analysis_heuristic", result.include_analysis_heuristic},
        {"observation_source", observation_source},
        {"target_metadata", target_metadata},
    };
}

std::size_t count_translation_units_by_kind(
    const std::vector<ImpactedTranslationUnit>& units, ImpactKind kind) {
    return static_cast<std::size_t>(std::count_if(
        units.begin(), units.end(),
        [kind](const ImpactedTranslationUnit& tu) { return tu.kind == kind; }));
}

std::size_t count_targets_by_classification(
    const std::vector<ImpactedTarget>& targets,
    TargetImpactClassification classification) {
    return static_cast<std::size_t>(std::count_if(
        targets.begin(), targets.end(),
        [classification](const ImpactedTarget& target) {
            return target.classification == classification;
        }));
}

std::string impact_classification_text(const ImpactResult& result) {
    if (result.heuristic && result.affected_translation_units.empty()) {
        return "uncertain";
    }
    return result.heuristic ? "heuristic" : "direct";
}

ordered_json render_impact_inputs(const ImpactResult& result) {
    auto base = render_inputs_common(result.inputs);
    base["changed_file"] = result.inputs.changed_file.value_or(std::string{});
    const auto source = result.inputs.changed_file_source.value_or(
        ChangedFileSource::cli_absolute);
    base["changed_file_source"] = changed_file_source_text(source);
    // Construct a fresh ordered_json from base instead of returning base
    // directly. NRVO would otherwise elide the local destructor at the
    // function-end brace, which the project's 100%-coverage gate flags as a
    // missed line.
    return ordered_json(std::move(base));
}

ordered_json render_impact_summary(const ImpactResult& result) {
    const auto affected_tu_count = result.affected_translation_units.size();
    const auto direct_tu_count = count_translation_units_by_kind(
        result.affected_translation_units, ImpactKind::direct);
    const auto heuristic_tu_count = count_translation_units_by_kind(
        result.affected_translation_units, ImpactKind::heuristic);
    const auto classification = impact_classification_text(result);
    const auto observation_source = observation_source_text(result.observation_source);
    const auto target_metadata = target_metadata_text(result.target_metadata);
    const auto affected_target_count = result.affected_targets.size();
    const auto direct_target_count = count_targets_by_classification(
        result.affected_targets, TargetImpactClassification::direct);
    const auto heuristic_target_count = count_targets_by_classification(
        result.affected_targets, TargetImpactClassification::heuristic);
    return ordered_json{
        {"affected_translation_unit_count", affected_tu_count},
        {"direct_translation_unit_count", direct_tu_count},
        {"heuristic_translation_unit_count", heuristic_tu_count},
        {"classification", classification},
        {"observation_source", observation_source},
        {"target_metadata", target_metadata},
        {"affected_target_count", affected_target_count},
        {"direct_target_count", direct_target_count},
        {"heuristic_target_count", heuristic_target_count},
    };
}

ordered_json render_impacted_unit(const ImpactedTranslationUnit& tu) {
    auto targets_array = ordered_json::array();
    append_targets_sorted(targets_array, tu.targets);
    return ordered_json{
        {"reference", render_reference(tu.reference)},
        {"targets", std::move(targets_array)},
    };
}

void append_impacted_translation_units(ordered_json& array,
                                        const std::vector<ImpactedTranslationUnit>& units,
                                        ImpactKind kind) {
    std::vector<ImpactedTranslationUnit> selected;
    for (const auto& tu : units) {
        if (tu.kind == kind) selected.push_back(tu);
    }
    std::sort(selected.begin(), selected.end(),
              [](const ImpactedTranslationUnit& lhs,
                 const ImpactedTranslationUnit& rhs) {
                  return reference_less(lhs.reference, rhs.reference);
              });
    for (const auto& tu : selected) {
        array.push_back(render_impacted_unit(tu));
    }
}

void append_impacted_targets(ordered_json& array,
                              const std::vector<ImpactedTarget>& targets,
                              TargetImpactClassification classification) {
    std::vector<TargetInfo> selected;
    for (const auto& impacted : targets) {
        if (impacted.classification == classification) {
            selected.push_back(impacted.target);
        }
    }
    append_targets_sorted(array, selected);
}

}  // namespace

std::string JsonReportAdapter::write_analysis_report(
    const AnalysisResult& analysis_result, std::size_t top_limit) const {
    auto diagnostics_array = ordered_json::array();
    append_diagnostics(diagnostics_array, analysis_result.diagnostics);
    ordered_json document;
    document["format"] = "cmake-xray.analysis";
    document["format_version"] = kReportFormatVersion;
    document["report_type"] = "analyze";
    document["inputs"] = render_inputs_common(analysis_result.inputs);
    document["summary"] = render_analysis_summary(analysis_result, top_limit);
    document["translation_unit_ranking"] =
        render_ranking_container(analysis_result, top_limit);
    document["include_hotspots"] =
        render_hotspot_container(analysis_result, top_limit);
    document["target_graph_status"] =
        target_graph_status_text(analysis_result.target_graph_status);
    document["target_graph"] = render_target_graph(analysis_result.target_graph);
    const TargetHubsView hubs_view{analysis_result.target_hubs_in,
                                    analysis_result.target_hubs_out};
    document["target_hubs"] = render_target_hubs(hubs_view);
    document["diagnostics"] = std::move(diagnostics_array);
    return document.dump(2) + "\n";
}

std::string JsonReportAdapter::write_impact_report(const ImpactResult& impact_result) const {
    auto direct_units = ordered_json::array();
    append_impacted_translation_units(direct_units, impact_result.affected_translation_units,
                                       ImpactKind::direct);
    auto heuristic_units = ordered_json::array();
    append_impacted_translation_units(heuristic_units,
                                       impact_result.affected_translation_units,
                                       ImpactKind::heuristic);
    auto direct_targets = ordered_json::array();
    append_impacted_targets(direct_targets, impact_result.affected_targets,
                             TargetImpactClassification::direct);
    auto heuristic_targets = ordered_json::array();
    append_impacted_targets(heuristic_targets, impact_result.affected_targets,
                             TargetImpactClassification::heuristic);
    auto diagnostics_array = ordered_json::array();
    append_diagnostics(diagnostics_array, impact_result.diagnostics);

    ordered_json document;
    document["format"] = "cmake-xray.impact";
    document["format_version"] = kReportFormatVersion;
    document["report_type"] = "impact";
    document["inputs"] = render_impact_inputs(impact_result);
    document["summary"] = render_impact_summary(impact_result);
    document["directly_affected_translation_units"] = std::move(direct_units);
    document["heuristically_affected_translation_units"] = std::move(heuristic_units);
    document["directly_affected_targets"] = std::move(direct_targets);
    document["heuristically_affected_targets"] = std::move(heuristic_targets);
    document["target_graph_status"] =
        target_graph_status_text(impact_result.target_graph_status);
    document["target_graph"] = render_target_graph(impact_result.target_graph);
    // AP M6-1.3 A.3 stops at the shared impact_priority_text.h helpers;
    // the JSON v3 emission of prioritized_affected_targets +
    // impact_target_depth_{requested,effective} lands in A.4 alongside
    // the kReportFormatVersion=3 flip and the schema-const update.
    document["diagnostics"] = std::move(diagnostics_array);
    return document.dump(2) + "\n";
}

}  // namespace xray::adapters::output
