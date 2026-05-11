#include "adapters/output/console_report_adapter.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "adapters/output/impact_priority_text.h"
#include "adapters/output/include_text_helpers.h"
#include "adapters/output/target_display_support.h"
#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_classification.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"
#include "hexagon/model/translation_unit.h"
#include "hexagon/services/target_graph_support.h"

namespace xray::adapters::output {

namespace {

using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraphStatus;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;
using xray::hexagon::model::TranslationUnitReference;

struct AnalysisSectionCounts {
    std::size_t ranking_count{0};
    std::size_t hotspot_count{0};
};

std::string diagnostic_prefix(DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::warning ? "warning" : "note";
}

std::string observation_source_text(xray::hexagon::model::ObservationSource source) {
    return source == xray::hexagon::model::ObservationSource::derived ? "derived" : "exact";
}

std::string target_metadata_text(TargetMetadataStatus status) {
    if (status == TargetMetadataStatus::loaded) {
        return "loaded";
    }
    if (status == TargetMetadataStatus::partial) {
        return "partial";
    }
    return "not_loaded";
}

bool target_metadata_loaded(TargetMetadataStatus status) {
    return target_metadata_text(status) != "not_loaded";
}

std::string target_label(const TargetInfo& target) {
    if (!target.display_name.empty()) return target.display_name;
    if (!target.unique_key.empty()) return target.unique_key;
    return target.type;
}

void append_target_suffix(std::ostringstream& out, const std::vector<TargetInfo>& targets) {
    if (targets.empty()) return;

    out << " [targets: ";
    for (std::size_t index = 0; index < targets.size(); ++index) {
        if (index != 0) out << ", ";
        out << target_label(targets[index]);
    }
    out << ']';
}

void append_translation_unit_reference(std::ostringstream& out,
                                       const TranslationUnitReference& reference) {
    out << reference.source_path << " [directory: " << reference.directory << ']';
}

void append_diagnostics(std::ostringstream& out, const std::vector<Diagnostic>& diagnostics,
                        std::string_view indent) {
    for (const auto& diagnostic : diagnostics) {
        out << indent << diagnostic_prefix(diagnostic.severity) << ": " << diagnostic.message
            << '\n';
    }
}

std::size_t count_mapped_translation_units(
    const std::vector<xray::hexagon::model::RankedTranslationUnit>& translation_units) {
    return static_cast<std::size_t>(
        std::count_if(translation_units.begin(), translation_units.end(),
                      [](const auto& tu) { return !tu.targets.empty(); }));
}

void append_analysis_target_overview(std::ostringstream& out,
                                     const AnalysisResult& analysis_result) {
    if (!target_metadata_loaded(analysis_result.target_metadata)) return;

    out << "observation source: " << observation_source_text(analysis_result.observation_source)
        << '\n';
    out << "target metadata: " << target_metadata_text(analysis_result.target_metadata) << '\n';
    out << "translation units with target mapping: "
        << count_mapped_translation_units(analysis_result.translation_units) << " of "
        << analysis_result.translation_units.size() << '\n';
}

AnalysisSectionCounts append_ranking_section(std::ostringstream& out,
                                             const AnalysisResult& analysis_result,
                                             std::size_t top_limit) {
    const auto ranking_count = std::min(top_limit, analysis_result.translation_units.size());

    out << "translation unit ranking\n";
    out << "based on metrics: arg_count + include_path_count + define_count\n";
    out << "top " << ranking_count << " of " << analysis_result.translation_units.size()
        << " translation units\n";

    for (std::size_t index = 0; index < ranking_count; ++index) {
        const auto& translation_unit = analysis_result.translation_units[index];
        out << translation_unit.rank << ". ";
        append_translation_unit_reference(out, translation_unit.reference);
        append_target_suffix(out, translation_unit.targets);
        out << '\n';
        out << "   arg_count=" << translation_unit.arg_count
            << " include_path_count=" << translation_unit.include_path_count
            << " define_count=" << translation_unit.define_count << '\n';
        append_diagnostics(out, translation_unit.diagnostics, "   ");
    }

    return {
        .ranking_count = ranking_count,
        .hotspot_count = std::min(top_limit, analysis_result.include_hotspots.size()),
    };
}

void append_hotspot_section(std::ostringstream& out, const AnalysisResult& result,
                            std::size_t hotspot_count) {
    // AP M6-1.4 A.5 step 23: v4 Console form pinned at plan step 23.1-23.5.
    // Heading carries the include-filter parenthetical; [heuristic] is
    // intentionally removed (M3 console marker no longer surfaces on the
    // Include Hotspots heading per plan step 23.2). Optional budget and
    // Showing lines render below the heading with two-space indent. Per
    // step 23.3 each hotspot is a single indented line; TU listing and
    // hotspot diagnostics are dropped (variant β, consistent with HTML
    // and Markdown).
    // Local scope/depth bindings mirror the HTML adapter pattern in
    // emit_hotspots_filter_line; without them gcov treats the first line
    // of the multi-line `out << ...` chain as a separate basic block
    // that the coverage gate marks uncovered.
    const auto scope_value = include_scope_text(result.include_scope_effective);
    const auto depth_value = include_depth_filter_text(result.include_depth_filter_effective);
    out << '\n';
    out << "Include Hotspots (scope=" << scope_value
        << ", depth=" << depth_value
        << "; excluded: " << result.include_hotspot_excluded_unknown_count
        << " unknown, " << result.include_hotspot_excluded_mixed_count << " mixed):\n";

    if (result.include_node_budget_reached) {
        out << "  Note: include analysis stopped at "
            << result.include_node_budget_effective << " nodes (budget reached).\n";
    }
    if (result.include_hotspots.size() > hotspot_count) {
        out << "  Showing " << hotspot_count << " of "
            << result.include_hotspots.size() << " include hotspots.\n";
    }

    if (result.include_hotspots.empty()) {
        out << "  No include hotspots found.\n";
        return;
    }

    for (std::size_t index = 0; index < hotspot_count; ++index) {
        const auto& hotspot = result.include_hotspots[index];
        out << "  " << hotspot.header_path << " ["
            << include_origin_text(hotspot.origin) << ", "
            << include_depth_kind_text(hotspot.depth_kind) << "] ("
            << hotspot.affected_translation_units.size() << " translation units)\n";
    }
}

void append_impact_target_overview(std::ostringstream& out, const ImpactResult& impact_result) {
    if (!target_metadata_loaded(impact_result.target_metadata)) return;

    out << "observation source: " << observation_source_text(impact_result.observation_source)
        << '\n';
    out << "target metadata: " << target_metadata_text(impact_result.target_metadata) << '\n';
    out << "affected targets: " << impact_result.affected_targets.size() << '\n';
}

void append_target(std::ostringstream& out, const TargetInfo& target) {
    out << "- " << target_label(target);
    if (!target.type.empty()) out << " [type: " << target.type << ']';
    out << '\n';
}

void append_target_section(std::ostringstream& out, std::string_view title,
                           TargetImpactClassification classification,
                           const ImpactResult& impact_result, std::string_view empty_text) {
    out << '\n' << title << '\n';

    bool any = false;
    for (const auto& impacted_target : impact_result.affected_targets) {
        if (impacted_target.classification != classification) continue;
        append_target(out, impacted_target.target);
        any = true;
    }

    if (!any) out << empty_text << '\n';
}

// ---- M6 AP 1.2 Tranche A.3: Target-Graph-related sections ---------------

std::string target_graph_status_text(TargetGraphStatus status) {
    // Caller filters not_loaded before reaching here; the not_loaded path
    // omits the entire section per docs/planning/plan-M6-1-2.md "Console- und
    // Markdown-Vertragsregeln". Mapping the remaining two enum values via
    // a ternary keeps the helper byte-stable without an unreachable third
    // branch that would otherwise miss coverage.
    return status == TargetGraphStatus::loaded ? "loaded" : "partial";
}

std::vector<std::string> render_from_column_with_suffix(
    const std::vector<TargetDependency>& edges) {
    // From-column has no [external] marker; use the shared display helper
    // verbatim per docs/planning/plan-M6-1-2.md "Adapter-Implementierungs-Hinweise".
    std::vector<TargetDisplayEntry> entries;
    entries.reserve(edges.size());
    for (const auto& edge : edges) {
        entries.push_back({edge.from_display_name, edge.from_unique_key});
    }
    return disambiguate_target_display_names(entries);
}

std::vector<std::string> render_to_column_with_suffixes(
    const std::vector<TargetDependency>& edges) {
    // Console-specific to-column: the suffix order is documented in
    // docs/planning/plan-M6-1-2.md "Reihenfolge der Suffixe bei kombiniertem
    // Mischfall: <to_display_name> [external] [key: <to_unique_key>]". The
    // shared disambiguation helper does not own the [external] suffix
    // because it knows nothing about edge resolution; assemble suffixes
    // here so the [external] marker always sits between the display name
    // and the [key:...] disambiguation marker.
    std::map<std::string, std::set<std::string>> distinct_keys_by_text;
    for (const auto& edge : edges) {
        distinct_keys_by_text[edge.to_display_name].insert(edge.to_unique_key);
    }
    std::vector<std::string> out;
    out.reserve(edges.size());
    for (const auto& edge : edges) {
        std::string assembled = edge.to_display_name;
        if (edge.resolution == TargetDependencyResolution::external) {
            assembled.append(" [external]");
        }
        if (distinct_keys_by_text.at(edge.to_display_name).size() > 1) {
            assembled.append(" [key: ").append(edge.to_unique_key).append("]");
        }
        out.push_back(std::move(assembled));
    }
    return out;
}

void append_target_graph_edge_lines(
    std::ostringstream& out, const std::vector<TargetDependency>& edges) {
    if (edges.empty()) {
        out << "  No direct target dependencies.\n";
        return;
    }
    const auto from_names = render_from_column_with_suffix(edges);
    const auto to_names = render_to_column_with_suffixes(edges);
    for (std::size_t i = 0; i < edges.size(); ++i) {
        out << "  " << from_names[i] << " -> " << to_names[i] << '\n';
    }
}

void append_target_graph_section(std::ostringstream& out, const AnalysisResult& result) {
    out << '\n';
    out << "Direct Target Dependencies (target_graph_status: "
        << target_graph_status_text(result.target_graph_status) << "):\n";
    append_target_graph_edge_lines(out, result.target_graph.edges);
}

void append_target_graph_reference_section(std::ostringstream& out,
                                            const ImpactResult& result) {
    out << '\n';
    out << "Target Graph Reference (target_graph_status: "
        << target_graph_status_text(result.target_graph_status) << "):\n";
    append_target_graph_edge_lines(out, result.target_graph.edges);
}

// AP M6-1.3 A.4: Impact-Console "Prioritised Affected Targets" section.
// Format per docs/planning/plan-M6-1-3.md "Console":
//   `[<priority_class>, distance=<n>, evidence=<strength>] <display_name>`
// One line per target; metadata-only header line for not_loaded.
void append_prioritised_affected_targets_section(std::ostringstream& out,
                                                  const ImpactResult& result) {
    out << '\n';
    if (result.target_graph_status == TargetGraphStatus::not_loaded) {
        out << "Prioritised Affected Targets (requested depth: "
            << result.impact_target_depth_requested
            << ", effective depth: 0, no graph).\n";
        return;
    }
    out << "Prioritised Affected Targets (requested depth: "
        << result.impact_target_depth_requested
        << ", effective depth: " << result.impact_target_depth_effective
        << "):\n";
    if (result.prioritized_affected_targets.empty()) {
        out << "  No prioritised targets.\n";
        return;
    }
    std::vector<TargetDisplayEntry> entries;
    entries.reserve(result.prioritized_affected_targets.size());
    for (const auto& t : result.prioritized_affected_targets) {
        entries.push_back({t.target.display_name, t.target.unique_key});
    }
    const auto names = disambiguate_target_display_names(entries);
    for (std::size_t i = 0; i < result.prioritized_affected_targets.size(); ++i) {
        const auto& t = result.prioritized_affected_targets[i];
        out << "  [" << priority_class_text_v3(t.priority_class)
            << ", distance=" << t.graph_distance
            << ", evidence=" << evidence_strength_text_v3(t.evidence_strength)
            << "] " << names[i] << '\n';
    }
}

// Bundle the hub-row parameters so `append_hub_line` stays inside the
// project's 5-parameter cap; mirrors the TargetHubsView struct in the
// JSON adapter.
struct ConsoleHubLine {
    std::string_view direction;       // "Inbound" or "Outbound"
    std::string_view direction_word;  // "incoming" or "outgoing"
    std::size_t threshold;
    const std::vector<TargetInfo>& hubs;
    std::string_view empty_text;
};

void append_hub_line(std::ostringstream& out, const ConsoleHubLine& line) {
    out << "  " << line.direction << " (>= " << line.threshold << " "
        << line.direction_word << "): ";
    if (line.hubs.empty()) {
        out << line.empty_text;
    } else {
        std::vector<TargetDisplayEntry> entries;
        entries.reserve(line.hubs.size());
        for (const auto& info : line.hubs) {
            entries.push_back({info.display_name, info.unique_key});
        }
        const auto names = disambiguate_target_display_names(entries);
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            out << names[i];
        }
    }
    out << '\n';
}

void append_target_hubs_section(std::ostringstream& out, const AnalysisResult& result) {
    out << '\n';
    constexpr auto in_threshold =
        xray::hexagon::services::kDefaultTargetHubInThreshold;
    constexpr auto out_threshold =
        xray::hexagon::services::kDefaultTargetHubOutThreshold;
    out << "Target Hubs (in_threshold: " << in_threshold
        << ", out_threshold: " << out_threshold << "):\n";
    append_hub_line(
        out, ConsoleHubLine{"Inbound", "incoming", in_threshold,
                            result.target_hubs_in, "No incoming hubs."});
    append_hub_line(
        out, ConsoleHubLine{"Outbound", "outgoing", out_threshold,
                            result.target_hubs_out, "No outgoing hubs."});
}

}  // namespace

std::string ConsoleReportAdapter::write_analysis_report(const AnalysisResult& analysis_result,
                                                        std::size_t top_limit) const {
    std::ostringstream out;

    append_analysis_target_overview(out, analysis_result);
    const auto counts = append_ranking_section(out, analysis_result, top_limit);
    append_hotspot_section(out, analysis_result, counts.hotspot_count);
    if (analysis_result.target_graph_status != TargetGraphStatus::not_loaded) {
        append_target_graph_section(out, analysis_result);
        append_target_hubs_section(out, analysis_result);
    }
    append_diagnostics(out, analysis_result.diagnostics, "");

    return out.str();
}

std::string ConsoleReportAdapter::write_impact_report(const ImpactResult& impact_result) const {
    std::ostringstream out;

    out << "impact analysis for " << impact_result.changed_file;
    if (impact_result.heuristic) out << " [heuristic]";
    out << '\n';
    out << "affected translation units: " << impact_result.affected_translation_units.size() << '\n';
    append_impact_target_overview(out, impact_result);

    for (const auto& impacted : impact_result.affected_translation_units) {
        out << "  ";
        append_translation_unit_reference(out, impacted.reference);
        append_target_suffix(out, impacted.targets);
        out << " [" << (impacted.kind == ImpactKind::direct ? "direct" : "heuristic") << "]\n";
    }

    if (target_metadata_loaded(impact_result.target_metadata)) {
        append_target_section(out, "directly affected targets",
                              TargetImpactClassification::direct, impact_result,
                              "no directly affected targets");
        append_target_section(out, "heuristically affected targets",
                              TargetImpactClassification::heuristic, impact_result,
                              "no heuristically affected targets");
    }

    if (impact_result.target_graph_status != TargetGraphStatus::not_loaded) {
        append_target_graph_reference_section(out, impact_result);
    }
    // AP M6-1.3 A.4: Prioritised Affected Targets is always present in
    // v3 console impact output, regardless of target_graph_status (the
    // not_loaded path renders the metadata-only header line).
    append_prioritised_affected_targets_section(out, impact_result);

    append_diagnostics(out, impact_result.diagnostics, "");

    return out.str();
}

}  // namespace xray::adapters::output
