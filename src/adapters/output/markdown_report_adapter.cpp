#include "adapters/output/markdown_report_adapter.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
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
using xray::hexagon::model::IncludeHotspot;
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

struct MarkdownEscapeContext {
    std::string_view text;
    std::size_t ordered_marker_dot{std::string_view::npos};
    bool leading_unordered_list_marker{false};
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

std::size_t ordered_list_marker_dot_index(std::string_view text) {
    std::size_t index = 0;
    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
        ++index;
    }

    if (index == 0 || index + 1 >= text.size() || text[index] != '.' || text[index + 1] != ' ') {
        return std::string_view::npos;
    }

    return index;
}

bool has_leading_unordered_list_marker(std::string_view text) {
    return text.starts_with("- ") || text.starts_with("+ ") || text.starts_with("* ");
}

bool is_markdown_escape_character(char ch) {
    switch (ch) {
    case '\\':
    case '`':
    case '*':
    case '_':
    case '[':
    case ']':
        return true;
    default:
        return false;
    }
}

bool should_escape_prefix(const MarkdownEscapeContext& context, std::size_t index) {
    if (index == context.ordered_marker_dot) return true;
    if (index != 0) return false;
    return context.text[index] == '#' || context.leading_unordered_list_marker;
}

void append_escaped_markdown(std::ostringstream& out, std::string_view text) {
    const MarkdownEscapeContext context{
        text,
        ordered_list_marker_dot_index(text),
        has_leading_unordered_list_marker(text),
    };

    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto ch = text[index];

        if (should_escape_prefix(context, index)) {
            out << '\\';
        }
        if (is_markdown_escape_character(ch)) {
            out << '\\';
        }
        out << ch;
    }
}

std::string ordered_list_indent(std::size_t index) {
    return std::string(std::to_string(index).size() + 3, ' ');
}

void append_reference(std::ostringstream& out, const TranslationUnitReference& reference) {
    append_escaped_markdown(out, reference.source_path);
    out << " [directory: ";
    append_escaped_markdown(out, reference.directory);
    out << ']';
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
        append_escaped_markdown(out, target_label(targets[index]));
    }
    out << ']';
}

void append_reference(
    std::ostringstream& out, const TranslationUnitReference& reference,
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key) {
    append_reference(out, reference);
    const auto it = targets_by_key.find(reference.unique_key);
    if (it != targets_by_key.end()) append_target_suffix(out, *it->second);
}

void append_diagnostic_list(std::ostringstream& out, const std::vector<Diagnostic>& diagnostics,
                            std::string_view indent) {
    for (const auto& diagnostic : diagnostics) {
        out << indent << "- " << diagnostic_prefix(diagnostic.severity) << ": ";
        append_escaped_markdown(out, diagnostic.message);
        out << '\n';
    }
}

std::size_t count_mapped_translation_units(
    const std::vector<xray::hexagon::model::RankedTranslationUnit>& translation_units) {
    return static_cast<std::size_t>(
        std::count_if(translation_units.begin(), translation_units.end(),
                      [](const auto& tu) { return !tu.targets.empty(); }));
}

void append_analysis_overview(std::ostringstream& out, const AnalysisResult& analysis_result,
                              std::size_t top_limit, const AnalysisSectionCounts& counts) {
    out << "- Report type: analyze\n";
    out << "- Compile database: ";
    append_escaped_markdown(out, analysis_result.compile_database_path);
    out << '\n';
    out << "- Translation units: " << analysis_result.translation_units.size() << '\n';
    out << "- Translation unit ranking entries: " << counts.ranking_count << " of "
        << analysis_result.translation_units.size() << '\n';
    out << "- Include hotspot entries: " << counts.hotspot_count << " of "
        << analysis_result.include_hotspots.size() << '\n';
    out << "- Top limit: " << top_limit << '\n';
    out << "- Include analysis heuristic: "
        << (analysis_result.include_analysis_heuristic ? "yes" : "no") << '\n';
    if (target_metadata_loaded(analysis_result.target_metadata)) {
        out << "- Observation source: "
            << observation_source_text(analysis_result.observation_source) << '\n';
        out << "- Target metadata: " << target_metadata_text(analysis_result.target_metadata)
            << '\n';
        out << "- Translation units with target mapping: "
            << count_mapped_translation_units(analysis_result.translation_units) << " of "
            << analysis_result.translation_units.size() << '\n';
    }
}

void append_ranking_section(std::ostringstream& out, const AnalysisResult& analysis_result,
                            std::size_t ranking_count) {
    out << "## Translation Unit Ranking\n";

    for (std::size_t index = 0; index < ranking_count; ++index) {
        const auto& translation_unit = analysis_result.translation_units[index];
        const auto indent = ordered_list_indent(translation_unit.rank);

        out << translation_unit.rank << ". ";
        append_reference(out, translation_unit.reference);
        append_target_suffix(out, translation_unit.targets);
        out << '\n';
        out << indent << "Metrics: arg_count=" << translation_unit.arg_count
            << ", include_path_count=" << translation_unit.include_path_count
            << ", define_count=" << translation_unit.define_count << '\n';

        if (translation_unit.diagnostics.empty()) continue;

        out << indent << "Diagnostics:\n";
        append_diagnostic_list(out, translation_unit.diagnostics, indent);
    }
}

// AP M6-1.4 A.5 step 22: see new tabular append_hotspot_section below,
// next to the AP 1.2 table-cell helpers it builds on. The old numbered-list
// hotspot rendering is removed in favour of the v4 contract pinned in
// docs/planning/in-progress/plan-M6-1-4.md step 22.

void append_report_diagnostics(std::ostringstream& out, const std::vector<Diagnostic>& diagnostics) {
    out << "## Diagnostics\n";
    if (diagnostics.empty()) {
        out << "No diagnostics.\n";
        return;
    }

    append_diagnostic_list(out, diagnostics, "");
}

std::string impact_classification(const ImpactResult& impact_result) {
    if (impact_result.heuristic && impact_result.affected_translation_units.empty()) {
        return "uncertain";
    }

    return impact_result.heuristic ? "heuristic" : "direct";
}

void append_impact_overview(std::ostringstream& out, const ImpactResult& impact_result) {
    out << "- Report type: impact\n";
    out << "- Compile database: ";
    append_escaped_markdown(out, impact_result.compile_database_path);
    out << '\n';
    out << "- Changed file: ";
    append_escaped_markdown(out, impact_result.changed_file);
    out << '\n';
    out << "- Affected translation units: " << impact_result.affected_translation_units.size()
        << '\n';
    out << "- Impact classification: " << impact_classification(impact_result) << '\n';
    if (target_metadata_loaded(impact_result.target_metadata)) {
        out << "- Observation source: "
            << observation_source_text(impact_result.observation_source) << '\n';
        out << "- Target metadata: " << target_metadata_text(impact_result.target_metadata)
            << '\n';
        out << "- Affected targets: " << impact_result.affected_targets.size() << '\n';
    }
}

void append_impacted_translation_unit_section(std::ostringstream& out, std::string_view title,
                                              ImpactKind kind,
                                              const ImpactResult& impact_result,
                                              std::string_view empty_text) {
    out << title << '\n';
    bool any = false;

    for (const auto& impacted : impact_result.affected_translation_units) {
        if (impacted.kind != kind) continue;
        out << "- ";
        append_reference(out, impacted.reference);
        append_target_suffix(out, impacted.targets);
        out << '\n';
        any = true;
    }

    if (!any) {
        out << empty_text << '\n';
    }
}

void append_target(std::ostringstream& out, const TargetInfo& target) {
    out << "- ";
    append_escaped_markdown(out, target_label(target));
    if (!target.type.empty()) {
        out << " [type: ";
        append_escaped_markdown(out, target.type);
        out << ']';
    }
    out << '\n';
}

void append_target_section(std::ostringstream& out, std::string_view title,
                           TargetImpactClassification classification,
                           const ImpactResult& impact_result, std::string_view empty_text) {
    out << title << '\n';
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

std::string target_dependency_resolution_text(TargetDependencyResolution res) {
    return res == TargetDependencyResolution::external ? "external" : "resolved";
}

std::string normalize_table_cell_whitespace(std::string_view value) {
    // Mirrors spec/report-html.md "Escaping" whitespace contract so M6
    // table cells stay byte-stable across HTML and Markdown for the same
    // inputs. \r\n and \r → " / ", each \n → " / ", \t → one space, other
    // ASCII control bytes < 0x20 → one space.
    std::string out;
    out.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const auto ch = static_cast<unsigned char>(value[index]);
        if (ch == '\r') {
            const auto next = index + 1;
            if (next < value.size() && value[next] == '\n') {
                ++index;
            }
            out.append(" / ");
            continue;
        }
        if (ch == '\n') {
            out.append(" / ");
            continue;
        }
        if (ch == '\t' || ch < 0x20) {
            out.push_back(' ');
            continue;
        }
        out.push_back(static_cast<char>(ch));
    }
    // NRVO-defeating return so the closing brace counts as a covered line
    // under gcov, mirroring src/adapters/output/dot_report_adapter.cpp Z. 291.
    return std::string(std::move(out));
}

void append_table_cell_plain(std::ostringstream& out, std::string_view value) {
    // Plain-text cell: whitespace-normalize per docs/planning/plan-M6-1-2.md
    // "Markdown-Tabellen-Escaping". No backtick wrapping. Callers supply
    // adapter-controlled fixed strings (Resolution values "resolved" /
    // "external", direction labels, hub leersaetze) that cannot contain
    // the column separator `|`; values that may carry `|` (target display
    // names, raw_ids) flow through append_table_cell_target instead, which
    // owns the escape rule alongside its backtick wrapper.
    out << normalize_table_cell_whitespace(value);
}

void append_table_cell_target(std::ostringstream& out, std::string_view value) {
    // Inline-code wrapped cell for target display names and raw_ids per
    // docs/planning/plan-M6-1-2.md "Markdown-Tabellen-Escaping". Whitespace-normalize
    // first; if the value carries a backtick, wrap with double backticks
    // and an inner space on each side so the inline-code span survives.
    // `|` is escaped to `\|` even inside the code span so renderers without
    // code-span pipe support still see one column.
    const auto normalized = normalize_table_cell_whitespace(value);
    const bool has_backtick = normalized.find('`') != std::string::npos;
    const std::string_view delim = has_backtick ? "``" : "`";
    out << delim;
    if (has_backtick) {
        out << ' ';
    }
    for (const char ch : normalized) {
        if (ch == '|') {
            out << "\\|";
        } else {
            out << ch;
        }
    }
    if (has_backtick) {
        out << ' ';
    }
    out << delim;
}

std::vector<std::string> disambiguate_edge_column(
    const std::vector<TargetDependency>& edges, bool from_column) {
    std::vector<TargetDisplayEntry> entries;
    entries.reserve(edges.size());
    for (const auto& edge : edges) {
        if (from_column) {
            entries.push_back({edge.from_display_name, edge.from_unique_key});
        } else {
            entries.push_back({edge.to_display_name, edge.to_unique_key});
        }
    }
    return disambiguate_target_display_names(entries);
}

void append_target_graph_table(std::ostringstream& out,
                                const xray::hexagon::model::TargetGraph& graph) {
    out << "| From | To | Resolution | External Target |\n";
    out << "|---|---|---|---|\n";
    const auto from_names = disambiguate_edge_column(graph.edges, /*from_column=*/true);
    const auto to_names = disambiguate_edge_column(graph.edges, /*from_column=*/false);
    for (std::size_t i = 0; i < graph.edges.size(); ++i) {
        const auto& edge = graph.edges[i];
        out << "| ";
        append_table_cell_target(out, from_names[i]);
        out << " | ";
        append_table_cell_target(out, to_names[i]);
        out << " | ";
        append_table_cell_plain(out, target_dependency_resolution_text(edge.resolution));
        out << " | ";
        if (edge.resolution == TargetDependencyResolution::external) {
            append_table_cell_target(out, edge.to_display_name);
        }
        out << " |\n";
    }
}

// AP M6-1.4 A.5 step 22: Context-cell composition helpers for the
// tabular Include Hotspots section. The Context cell carries free-form
// text per docs/planning/in-progress/plan-M6-1-4.md step 22.4 (variant A,
// flat TU list with " / " separator) so it does NOT wrap in backticks
// like append_table_cell_target; it still inherits the AP 1.2 table-cell
// escaping for whitespace and the `|` column-boundary character.

void append_context_value(std::ostringstream& out, std::string_view value) {
    std::ostringstream value_stream;
    append_escaped_markdown(value_stream, value);
    const auto normalized = normalize_table_cell_whitespace(value_stream.str());
    for (const char ch : normalized) {
        if (ch == '|') {
            out << "\\|";
        } else {
            out << ch;
        }
    }
}

void append_hotspot_context_cell(
    std::ostringstream& out, const IncludeHotspot& hotspot,
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key) {
    // Per-TU shape: <source_path> [directory: <directory>] plus, when
    // target metadata is loaded, [targets: <name1>, <name2>] with
    // disambiguated display names per plan step 22.6 (variant Z). The
    // inter-TU separator is " / " mirroring normalize_table_cell_whitespace.
    bool first = true;
    for (const auto& reference : hotspot.affected_translation_units) {
        if (!first) {
            out << " / ";
        }
        first = false;
        append_context_value(out, reference.source_path);
        out << " [directory: ";
        append_context_value(out, reference.directory);
        out << "]";
        const auto it = targets_by_key.find(reference.unique_key);
        if (it == targets_by_key.end() || it->second->empty()) {
            continue;
        }
        std::vector<TargetDisplayEntry> entries;
        entries.reserve(it->second->size());
        for (const auto& target : *it->second) {
            entries.push_back({target_label(target), target.unique_key});
        }
        const auto disambiguated = disambiguate_target_display_names(entries);
        out << " [targets: ";
        for (std::size_t index = 0; index < disambiguated.size(); ++index) {
            if (index > 0) {
                out << ", ";
            }
            append_context_value(out, disambiguated[index]);
        }
        out << "]";
    }
}

void append_hotspot_filter_paragraph(std::ostringstream& out, const AnalysisResult& result) {
    // Filter paragraph as pinned in plan step 22.1: backticks around the
    // scope/depth values AND around the excluded counts (the form
    // unified at docs/planning/in-progress/plan-M6-1-4.md:866).
    out << "Filter: `scope=" << include_scope_text(result.include_scope_effective)
        << "`, `depth=" << include_depth_filter_text(result.include_depth_filter_effective)
        << "`. Excluded: `" << result.include_hotspot_excluded_unknown_count
        << "` unknown, `" << result.include_hotspot_excluded_mixed_count
        << "` mixed.\n";
}

void append_hotspot_section(std::ostringstream& out, const AnalysisResult& result,
                            std::size_t hotspot_count) {
    // AP M6-1.4 A.5 step 22: v4 tabular form pinned at plan step 22.1-22.6.
    // Order: heading, filter paragraph, optional budget note, optional
    // top-limit note, then either empty marker OR the hotspot table.
    // Filter and optional budget paragraphs ALSO render for the
    // empty-include_hotspots case (step 22.2) because they reflect
    // analysis configuration, not the result set.
    std::map<std::string, const std::vector<TargetInfo>*> targets_by_key;
    for (const auto& assignment : result.target_assignments) {
        targets_by_key.emplace(assignment.observation_key, &assignment.targets);
    }

    out << "## Include Hotspots\n\n";
    append_hotspot_filter_paragraph(out, result);
    if (result.include_node_budget_reached) {
        out << "\nNote: include analysis stopped at "
            << result.include_node_budget_effective
            << " nodes (budget reached).\n";
    }
    if (result.include_hotspots.size() > hotspot_count) {
        out << "\nShowing " << hotspot_count << " of "
            << result.include_hotspots.size() << " include hotspots.\n";
    }

    if (result.include_hotspots.empty()) {
        out << "\nNo include hotspots found.\n";
        return;
    }

    out << "\n| Header | Origin | Depth | Affected TUs | Context |\n";
    out << "|---|---|---|---|---|\n";
    for (std::size_t index = 0; index < hotspot_count; ++index) {
        const auto& hotspot = result.include_hotspots[index];
        out << "| ";
        append_table_cell_target(out, hotspot.header_path);
        out << " | ";
        append_table_cell_target(out, include_origin_text(hotspot.origin));
        out << " | ";
        append_table_cell_target(out, include_depth_kind_text(hotspot.depth_kind));
        out << " | " << hotspot.affected_translation_units.size() << " | ";
        append_hotspot_context_cell(out, hotspot, targets_by_key);
        out << " |\n";
    }
}

void append_target_graph_section(std::ostringstream& out, const AnalysisResult& result) {
    // Caller guarantees status != not_loaded; the not_loaded branch omits the
    // entire section per docs/planning/plan-M6-1-2.md "Console- und
    // Markdown-Vertragsregeln".
    out << "## Direct Target Dependencies\n\n";
    out << "Status: `" << target_graph_status_text(result.target_graph_status) << "`.\n\n";
    if (result.target_graph.edges.empty()) {
        out << "No direct target dependencies.\n";
        return;
    }
    append_target_graph_table(out, result.target_graph);
}

void append_hub_row(std::ostringstream& out, std::string_view direction,
                    std::size_t threshold, const std::vector<TargetInfo>& hubs,
                    std::string_view empty_text) {
    out << "| " << direction << " | " << threshold << " | ";
    if (hubs.empty()) {
        append_table_cell_plain(out, empty_text);
    } else {
        std::vector<TargetDisplayEntry> entries;
        entries.reserve(hubs.size());
        for (const auto& info : hubs) {
            entries.push_back({info.display_name, info.unique_key});
        }
        const auto names = disambiguate_target_display_names(entries);
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            append_table_cell_target(out, names[i]);
        }
    }
    out << " |\n";
}

void append_target_hubs_section(std::ostringstream& out, const AnalysisResult& result) {
    // Caller guarantees status != not_loaded.
    out << "## Target Hubs\n\n";
    out << "| Direction | Threshold | Targets |\n";
    out << "|---|---|---|\n";
    append_hub_row(out, "Inbound",
                   xray::hexagon::services::kDefaultTargetHubInThreshold,
                   result.target_hubs_in, "No incoming hubs.");
    append_hub_row(out, "Outbound",
                   xray::hexagon::services::kDefaultTargetHubOutThreshold,
                   result.target_hubs_out, "No outgoing hubs.");
}

void append_target_graph_reference_section(std::ostringstream& out,
                                            const ImpactResult& result) {
    // Caller guarantees status != not_loaded.
    out << "## Target Graph Reference\n\n";
    out << "Status: `" << target_graph_status_text(result.target_graph_status) << "`.\n\n";
    if (result.target_graph.edges.empty()) {
        out << "No direct target dependencies.\n";
        return;
    }
    append_target_graph_table(out, result.target_graph);
}

// AP M6-1.3 A.4: Impact-Markdown "Prioritised Affected Targets" section
// per docs/planning/plan-M6-1-3.md "Markdown". Always present in v3 impact
// output; body shape varies by target_graph_status.
void append_prioritised_affected_targets_section(std::ostringstream& out,
                                                  const ImpactResult& result) {
    out << "## Prioritised Affected Targets\n\n";
    if (result.target_graph_status == TargetGraphStatus::not_loaded) {
        out << "Requested depth: `" << result.impact_target_depth_requested
            << "`. Effective depth: `0` (no graph).\n\n";
        out << "Target graph not loaded; prioritisation skipped.\n";
        return;
    }
    out << "Requested depth: `" << result.impact_target_depth_requested
        << "`. Effective depth: `" << result.impact_target_depth_effective
        << "`.\n\n";
    if (result.prioritized_affected_targets.empty()) {
        out << "No prioritised targets.\n";
        return;
    }
    out << "| Display name | Type | Priority class | Graph distance | Evidence strength | Unique key |\n";
    out << "|---|---|---|---|---|---|\n";
    for (const auto& entry : result.prioritized_affected_targets) {
        out << "| ";
        append_table_cell_target(out, entry.target.display_name);
        out << " | ";
        append_table_cell_plain(out, entry.target.type);
        out << " | ";
        append_table_cell_plain(out, priority_class_text_v3(entry.priority_class));
        out << " | " << entry.graph_distance << " | ";
        append_table_cell_plain(out, evidence_strength_text_v3(entry.evidence_strength));
        out << " | ";
        append_table_cell_target(out, entry.target.unique_key);
        out << " |\n";
    }
}

}  // namespace

std::string MarkdownReportAdapter::write_analysis_report(const AnalysisResult& analysis_result,
                                                         std::size_t top_limit) const {
    std::ostringstream out;
    const auto counts = AnalysisSectionCounts{
        .ranking_count = std::min(top_limit, analysis_result.translation_units.size()),
        .hotspot_count = std::min(top_limit, analysis_result.include_hotspots.size()),
    };

    out << "# Project Analysis Report\n\n";
    append_analysis_overview(out, analysis_result, top_limit, counts);
    out << '\n';
    append_ranking_section(out, analysis_result, counts.ranking_count);
    out << '\n';
    append_hotspot_section(out, analysis_result, counts.hotspot_count);
    if (analysis_result.target_graph_status != TargetGraphStatus::not_loaded) {
        out << '\n';
        append_target_graph_section(out, analysis_result);
        out << '\n';
        append_target_hubs_section(out, analysis_result);
    }
    out << '\n';
    append_report_diagnostics(out, analysis_result.diagnostics);

    return out.str();
}

std::string MarkdownReportAdapter::write_impact_report(const ImpactResult& impact_result) const {
    std::ostringstream out;

    out << "# Impact Analysis Report\n\n";
    append_impact_overview(out, impact_result);

    if (impact_result.affected_translation_units.empty()) {
        out << "\nNo affected translation units found.\n";
    }

    out << '\n';
    append_impacted_translation_unit_section(
        out, "## Directly Affected Translation Units", ImpactKind::direct, impact_result,
        "No directly affected translation units.");
    out << '\n';
    append_impacted_translation_unit_section(
        out, "## Heuristically Affected Translation Units", ImpactKind::heuristic,
        impact_result, "No heuristically affected translation units.");
    if (target_metadata_loaded(impact_result.target_metadata)) {
        out << '\n';
        append_target_section(out, "## Directly Affected Targets",
                              TargetImpactClassification::direct, impact_result,
                              "No directly affected targets.");
        out << '\n';
        append_target_section(out, "## Heuristically Affected Targets",
                              TargetImpactClassification::heuristic, impact_result,
                              "No heuristically affected targets.");
    }
    if (impact_result.target_graph_status != TargetGraphStatus::not_loaded) {
        out << '\n';
        append_target_graph_reference_section(out, impact_result);
    }
    // AP M6-1.3 A.4: Prioritised section appears in every v3 impact
    // output, regardless of target_graph_status.
    out << '\n';
    append_prioritised_affected_targets_section(out, impact_result);
    out << '\n';
    append_report_diagnostics(out, impact_result.diagnostics);

    return out.str();
}

}  // namespace xray::adapters::output
