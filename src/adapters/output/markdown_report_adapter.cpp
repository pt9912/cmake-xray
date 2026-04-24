#include "adapters/output/markdown_report_adapter.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <string_view>
#include <vector>

#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/target_info.h"
#include "hexagon/model/translation_unit.h"

namespace xray::adapters::output {

namespace {

using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
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

void append_hotspot_section(std::ostringstream& out, const AnalysisResult& analysis_result,
                            std::size_t hotspot_count) {
    std::map<std::string, const std::vector<TargetInfo>*> targets_by_key;
    for (const auto& assignment : analysis_result.target_assignments) {
        targets_by_key.emplace(assignment.observation_key, &assignment.targets);
    }

    out << "## Include Hotspots\n";

    if (analysis_result.include_hotspots.empty()) {
        out << "No include hotspots found.\n";
        return;
    }

    for (std::size_t index = 0; index < hotspot_count; ++index) {
        const auto item_index = index + 1;
        const auto indent = ordered_list_indent(item_index);
        const auto& hotspot = analysis_result.include_hotspots[index];

        out << item_index << ". Header: ";
        append_escaped_markdown(out, hotspot.header_path);
        out << '\n';
        out << indent << "Affected translation units: "
            << hotspot.affected_translation_units.size() << '\n';
        out << indent << "Translation units:\n";

        for (const auto& reference : hotspot.affected_translation_units) {
            out << indent << "- ";
            append_reference(out, reference, targets_by_key);
            out << '\n';
        }

        if (hotspot.diagnostics.empty()) continue;

        out << indent << "Diagnostics:\n";
        append_diagnostic_list(out, hotspot.diagnostics, indent);
    }
}

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
    out << '\n';
    append_report_diagnostics(out, impact_result.diagnostics);

    return out.str();
}

}  // namespace xray::adapters::output
