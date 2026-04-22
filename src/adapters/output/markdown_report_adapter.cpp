#include "adapters/output/markdown_report_adapter.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>

#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/translation_unit.h"

namespace xray::adapters::output {

namespace {

using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
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

std::string escape_markdown(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() * 2);

    const MarkdownEscapeContext context{
        text,
        ordered_list_marker_dot_index(text),
        has_leading_unordered_list_marker(text),
    };

    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto ch = text[index];

        if (should_escape_prefix(context, index)) {
            escaped.push_back('\\');
        }
        if (is_markdown_escape_character(ch)) {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }

    return escaped; }  // GCOVR_EXCL_LINE

std::string ordered_list_indent(std::size_t index) {
    return std::string(std::to_string(index).size() + 3, ' ');
}

void append_reference(std::ostringstream& out, const TranslationUnitReference& reference) {
    out << escape_markdown(reference.source_path) << " [directory: "
        << escape_markdown(reference.directory) << ']';
}

void append_diagnostic_list(std::ostringstream& out, const std::vector<Diagnostic>& diagnostics,
                            std::string_view indent) {
    for (const auto& diagnostic : diagnostics) {
        out << indent << "- " << diagnostic_prefix(diagnostic.severity) << ": "
            << escape_markdown(diagnostic.message) << '\n';
    }
}

void append_analysis_overview(std::ostringstream& out, const AnalysisResult& analysis_result,
                              std::size_t top_limit, const AnalysisSectionCounts& counts) {
    out << "- Report type: analyze\n";
    out << "- Compile database: " << escape_markdown(analysis_result.compile_database_path) << '\n';
    out << "- Translation units: " << analysis_result.translation_units.size() << '\n';
    out << "- Translation unit ranking entries: " << counts.ranking_count << " of "
        << analysis_result.translation_units.size() << '\n';
    out << "- Include hotspot entries: " << counts.hotspot_count << " of "
        << analysis_result.include_hotspots.size() << '\n';
    out << "- Top limit: " << top_limit << '\n';
    out << "- Include analysis heuristic: "
        << (analysis_result.include_analysis_heuristic ? "yes" : "no") << '\n';
}

void append_ranking_section(std::ostringstream& out, const AnalysisResult& analysis_result,
                            std::size_t ranking_count) {
    out << "## Translation Unit Ranking\n";

    for (std::size_t index = 0; index < ranking_count; ++index) {
        const auto& translation_unit = analysis_result.translation_units[index];
        const auto indent = ordered_list_indent(translation_unit.rank);

        out << translation_unit.rank << ". ";
        append_reference(out, translation_unit.reference);
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
    out << "## Include Hotspots\n";

    if (analysis_result.include_hotspots.empty()) {
        out << "No include hotspots found.\n";
        return;
    }

    for (std::size_t index = 0; index < hotspot_count; ++index) {
        const auto item_index = index + 1;
        const auto indent = ordered_list_indent(item_index);
        const auto& hotspot = analysis_result.include_hotspots[index];

        out << item_index << ". Header: " << escape_markdown(hotspot.header_path) << '\n';
        out << indent << "Affected translation units: "
            << hotspot.affected_translation_units.size() << '\n';
        out << indent << "Translation units:\n";

        for (const auto& reference : hotspot.affected_translation_units) {
            out << indent << "- ";
            append_reference(out, reference);
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
    out << "- Compile database: " << escape_markdown(impact_result.compile_database_path) << '\n';
    out << "- Changed file: " << escape_markdown(impact_result.changed_file) << '\n';
    out << "- Affected translation units: " << impact_result.affected_translation_units.size()
        << '\n';
    out << "- Impact classification: " << impact_classification(impact_result) << '\n';
}

void append_impact_section(std::ostringstream& out, std::string_view title,
                           const std::vector<TranslationUnitReference>& references,
                           std::string_view empty_text) {
    out << title << '\n';
    if (references.empty()) {
        out << empty_text << '\n';
        return;
    }

    for (const auto& reference : references) {
        out << "- ";
        append_reference(out, reference);
        out << '\n';
    }
}

std::vector<TranslationUnitReference> collect_impact_references(
    const ImpactResult& impact_result, ImpactKind kind) {
    std::vector<TranslationUnitReference> references;

    for (const auto& impacted : impact_result.affected_translation_units) {
        if (impacted.kind != kind) continue;
        references.push_back(impacted.reference);
    }

    return references; }  // GCOVR_EXCL_LINE

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
    const auto direct_references =
        collect_impact_references(impact_result, ImpactKind::direct);
    const auto heuristic_references =
        collect_impact_references(impact_result, ImpactKind::heuristic);

    out << "# Impact Analysis Report\n\n";
    append_impact_overview(out, impact_result);

    if (impact_result.affected_translation_units.empty()) {
        out << "\nNo affected translation units found.\n";
    }

    out << '\n';
    append_impact_section(out, "## Directly Affected Translation Units", direct_references,
                          "No directly affected translation units.");
    out << '\n';
    append_impact_section(out, "## Heuristically Affected Translation Units",
                          heuristic_references, "No heuristically affected translation units.");
    out << '\n';
    append_report_diagnostics(out, impact_result.diagnostics);

    return out.str();
}

}  // namespace xray::adapters::output
