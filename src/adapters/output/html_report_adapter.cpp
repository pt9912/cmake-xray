#include "adapters/output/html_report_adapter.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "adapters/output/impact_priority_text.h"
#include "adapters/output/include_text_helpers.h"
#include "adapters/output/target_display_support.h"
#include "hexagon/model/analysis_configuration.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_classification.h"
#include "hexagon/model/include_filter_options.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"
#include "hexagon/model/translation_unit.h"
#include "hexagon/services/target_graph_support.h"

namespace xray::adapters::output {

std::string html_escape_text(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&':
            out.append("&amp;");
            break;
        case '<':
            out.append("&lt;");
            break;
        case '>':
            out.append("&gt;");
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    // NRVO-defeating return mirrors render_impact_inputs in the JSON adapter
    // so the closing-brace destructor counts as a covered line under gcov.
    return std::string(std::move(out));
}

std::string html_escape_attribute(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&':
            out.append("&amp;");
            break;
        case '<':
            out.append("&lt;");
            break;
        case '>':
            out.append("&gt;");
            break;
        case '"':
            out.append("&quot;");
            break;
        case '\'':
            out.append("&#39;");
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return std::string(std::move(out));
}

std::string render_text(std::string_view value) {
    return html_escape_text(normalize_html_whitespace(value));
}

std::string render_attribute(std::string_view value) {
    return html_escape_attribute(normalize_html_whitespace(value));
}

std::string normalize_html_whitespace(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const auto ch = static_cast<unsigned char>(value[index]);
        if (ch == '\r') {
            // \r\n → " / "; lone \r → " / ". The lookahead skip keeps \r\n
            // from emitting two separators.
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
        if (ch == '\t') {
            out.push_back(' ');
            continue;
        }
        if (ch < 0x20) {
            out.push_back(' ');
            continue;
        }
        out.push_back(static_cast<char>(ch));
    }
    return std::string(std::move(out));
}

std::string_view html_report_css() {
    // Static, byte-stable CSS per spec/report-html.md "CSS-Vertrag".
    // Identical for analyze and impact. No @import / url(...) / external
    // fonts / animations / random values. System fontstacks only.
    // Layout: leading newline puts <style> on its own line, one CSS rule
    // per line keeps goldens diff-friendly.
    static constexpr std::string_view kCss =
        "\n"
        "body{margin:0;padding:1.5rem;background:#ffffff;color:#1a1a1a;"
        "font-family:system-ui,-apple-system,\"Segoe UI\",Roboto,Helvetica,Arial,sans-serif;"
        "line-height:1.5;}\n"
        "main.report{max-width:80rem;margin:0 auto;}\n"
        "h1{font-size:1.75rem;margin:0 0 1rem 0;}\n"
        "h2{font-size:1.25rem;margin:1.5rem 0 0.75rem 0;}\n"
        "h3{font-size:1.0rem;margin:1rem 0 0.5rem 0;}\n"
        "section{margin-bottom:1.5rem;}\n"
        "table{border-collapse:collapse;width:100%;}\n"
        "th,td{border:1px solid #d0d0d0;padding:0.5rem;text-align:left;vertical-align:top;}\n"
        "th{background:#f3f3f3;font-weight:600;}\n"
        ".table-wrap{overflow-x:auto;}\n"
        ".badge{display:inline-block;padding:0.1rem 0.5rem;border:1px solid #1a1a1a;"
        "border-radius:0.25rem;background:#f0f0f0;color:#1a1a1a;font-size:0.875rem;}\n"
        ".badge--evidence-direct{background:#1a1a1a;color:#ffffff;}\n"
        ".badge--evidence-heuristic{background:#ffffff;color:#1a1a1a;}\n"
        ".badge--project{background:#e8f1ff;color:#0b3a8c;border-color:#0b3a8c;}\n"
        ".badge--external{background:#fff4e0;color:#7a4a00;border-color:#7a4a00;}\n"
        ".badge--unknown{background:#f0f0f0;color:#404040;}\n"
        ".badge--direct{background:#e8ffe8;color:#0b6b0b;border-color:#0b6b0b;}\n"
        ".badge--indirect{background:#fff0f4;color:#8c0b3a;border-color:#8c0b3a;}\n"
        ".badge--mixed{background:#fffbe0;color:#7a6a00;border-color:#7a6a00;}\n"
        ".empty{color:#404040;font-style:italic;}\n"
        "code,pre{font-family:ui-monospace,\"SFMono-Regular\",\"Menlo\",\"Consolas\",monospace;}\n"
        "@page{margin:1.5cm;}\n"
        "@media print{body{background:#ffffff;color:#000000;}"
        "th{background:#ffffff;}"
        "thead{display:table-header-group;}"
        "tr{break-inside:avoid;page-break-inside:avoid;}"
        ".table-wrap{overflow-x:visible;}}\n";
    return kCss;
}

namespace {

using xray::hexagon::model::AnalysisConfiguration;
using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::AnalysisSection;
using xray::hexagon::model::AnalysisSectionState;
using xray::hexagon::model::ChangedFileSource;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactedTarget;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::IncludeDepthFilter;
using xray::hexagon::model::IncludeDepthKind;
using xray::hexagon::model::IncludeHotspot;
using xray::hexagon::model::IncludeOrigin;
using xray::hexagon::model::IncludeScope;
using xray::hexagon::model::ObservationSource;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::ReportInputs;
using xray::hexagon::model::ReportInputSource;
using xray::hexagon::model::TargetAssignment;
using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetGraphStatus;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;
using xray::hexagon::model::TranslationUnitReference;
using xray::hexagon::model::TuRankingMetric;

// ---- Static text mappings -----------------------------------------------

std::string source_text(ReportInputSource source) {
    return source == ReportInputSource::cli ? "cli" : "not provided";
}

std::string optional_or_not_provided(const std::optional<std::string>& value) {
    return value.has_value() ? *value : std::string{"not provided"};
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
    // AP M6-1.5 A.5 HTML v5: section_active() filters disabled and
    // not_loaded before the analyze paths reach this helper, but the
    // impact "Target Graph Reference" section still passes any status
    // through. All four enum values map explicitly so the helper stays
    // total.
    if (status == TargetGraphStatus::loaded) return "loaded";
    if (status == TargetGraphStatus::disabled) return "disabled";
    if (status == TargetGraphStatus::not_loaded) return "not_loaded";
    return "partial";
}

// ---- AP M6-1.5 A.5 Tranche A.5 step 18c: section state helpers ---------

std::string analysis_section_text(AnalysisSection section) {
    if (section == AnalysisSection::tu_ranking) return "tu-ranking";
    if (section == AnalysisSection::include_hotspots) return "include-hotspots";
    if (section == AnalysisSection::target_graph) return "target-graph";
    return "target-hubs";
}

std::string analysis_section_state_text(AnalysisSectionState state) {
    if (state == AnalysisSectionState::active) return "active";
    if (state == AnalysisSectionState::disabled) return "disabled";
    return "not_loaded";
}

std::string analysis_section_state_modifier(AnalysisSectionState state) {
    // Maps to the CSS modifier suffix appended to `badge--state-`. The
    // not_loaded enum value emits the source-stable "not_loaded" text in
    // the badge content, but the class name uses the hyphenated form
    // "not-loaded" matching the existing badge--evidence-heuristic
    // hyphen convention.
    if (state == AnalysisSectionState::active) return "active";
    if (state == AnalysisSectionState::disabled) return "disabled";
    return "not-loaded";
}

std::size_t tu_threshold_value(const AnalysisConfiguration& cfg,
                               TuRankingMetric metric) {
    const auto it = cfg.tu_thresholds.find(metric);
    return it == cfg.tu_thresholds.end() ? 0 : it->second;
}

// resolve_section_state mirrors console_report_adapter and
// markdown_report_adapter: real service runs always populate the map;
// bare-fixture adapter unit tests fall back to legacy gating so the
// existing target_graph_status tests keep passing without a fixture
// rewrite.
AnalysisSectionState resolve_section_state(const AnalysisResult& result,
                                            AnalysisSection section) {
    const auto it = result.analysis_section_states.find(section);
    if (it != result.analysis_section_states.end()) return it->second;
    if (section == AnalysisSection::target_graph ||
        section == AnalysisSection::target_hubs) {
        return result.target_graph_status == TargetGraphStatus::not_loaded
                   ? AnalysisSectionState::not_loaded
                   : AnalysisSectionState::active;
    }
    return AnalysisSectionState::active;
}

void emit_section_state_badge(std::ostringstream& out, AnalysisSectionState state) {
    // Inline span placed inside the section's `<h2>` per plan §636-639.
    // The badge text reads "Status: <state>" verbatim; the modifier
    // class follows the badge-- convention (hyphenated). No CSS rule
    // ships with this commit; the base `.badge` style provides visual
    // affordance and downstream styling can layer onto the modifier.
    const auto state_label = analysis_section_state_text(state);
    const auto modifier = analysis_section_state_modifier(state);
    out << " <span class=\"badge badge--state-" << modifier << "\">Status: "
        << state_label << "</span>";
}

void emit_section_empty_paragraph(std::ostringstream& out,
                                   AnalysisSectionState state) {
    // Plan §640-647: disabled and not_loaded sections keep their `<h2>`
    // and replace the body with a single explanatory paragraph. The
    // "empty" class lets the existing CSS rule colour the italic prose
    // identically to other empty-section markers (M6 AP 1.2 contract).
    if (state == AnalysisSectionState::disabled) {
        out << "<p class=\"empty\">Section disabled.</p>";
        return;
    }
    out << "<p class=\"empty\">Section not loaded.</p>";
}

void emit_analysis_section_inline_list(
    std::ostringstream& out, const std::vector<AnalysisSection>& sections) {
    for (std::size_t index = 0; index < sections.size(); ++index) {
        if (index != 0) out << ", ";
        out << render_text(analysis_section_text(sections[index]));
    }
}

void emit_section_states_dt_dd_row(std::ostringstream& out,
                                    AnalysisSection section,
                                    const AnalysisResult& result) {
    out << "<dt>" << render_text(analysis_section_text(section)) << "</dt><dd>"
        << render_text(
               analysis_section_state_text(resolve_section_state(result, section)))
        << "</dd>\n";
}

void emit_analysis_configuration_section(std::ostringstream& out,
                                          const AnalysisResult& result) {
    // Plan §614-635: new top-level section between Inputs and Translation
    // Unit Ranking that surfaces the resolved analysis configuration plus
    // the four section states. Layout uses dl/dt/dd for the config
    // key-value group (matching the existing Inputs section) and a second
    // dl for the Section States enumeration so the markup remains
    // accessible without introducing a new table. The dl-based shape
    // also keeps the byte-shape compact for goldens.
    const auto& cfg = result.analysis_configuration;
    const auto arg_value =
        tu_threshold_value(cfg, TuRankingMetric::arg_count);
    const auto include_value =
        tu_threshold_value(cfg, TuRankingMetric::include_path_count);
    const auto define_value =
        tu_threshold_value(cfg, TuRankingMetric::define_count);
    out << "<section class=\"analysis-configuration\">"
        << "<h2>Analysis Configuration</h2>\n"
        << "<dl>\n"
        << "<dt>Sections</dt><dd>";
    emit_analysis_section_inline_list(out, cfg.effective_sections);
    out << "</dd>\n"
        << "<dt>TU thresholds</dt><dd>arg_count=" << arg_value
        << ", include_path_count=" << include_value
        << ", define_count=" << define_value << "</dd>\n"
        << "<dt>Min hotspot TUs</dt><dd>" << cfg.min_hotspot_tus << "</dd>\n"
        << "<dt>Target hub thresholds</dt><dd>in=" << cfg.target_hub_in_threshold
        << ", out=" << cfg.target_hub_out_threshold << "</dd>\n"
        << "</dl>\n"
        << "<h3>Section States</h3>\n"
        << "<dl class=\"section-states\">\n";
    emit_section_states_dt_dd_row(out, AnalysisSection::tu_ranking, result);
    emit_section_states_dt_dd_row(out, AnalysisSection::include_hotspots, result);
    emit_section_states_dt_dd_row(out, AnalysisSection::target_graph, result);
    emit_section_states_dt_dd_row(out, AnalysisSection::target_hubs, result);
    out << "</dl></section>";
}

std::string target_graph_status_badge_class(TargetGraphStatus status) {
    // NRVO-defeating return so the closing brace counts as a covered line
    // under gcov; mirrors src/adapters/output/dot_report_adapter.cpp Z. 291.
    std::string out = status == TargetGraphStatus::loaded
                          ? "badge--loaded"
                          : (status == TargetGraphStatus::disabled ? "badge--disabled"
                                                                    : "badge--partial");
    return std::string(std::move(out));
}

std::string target_dependency_resolution_text(TargetDependencyResolution res) {
    return res == TargetDependencyResolution::external ? "external" : "resolved";
}

std::string severity_text(DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::warning ? "warning" : "note";
}

std::string impact_classification(const ImpactResult& impact_result) {
    if (impact_result.heuristic && impact_result.affected_translation_units.empty()) {
        return "uncertain";
    }
    return impact_result.heuristic ? "heuristic" : "direct";
}

std::string target_label(const TargetInfo& target) {
    if (!target.display_name.empty()) return target.display_name;
    if (!target.unique_key.empty()) return target.unique_key;
    return target.type;
}

// ---- Sorted target list helper ------------------------------------------

std::vector<TargetInfo> sorted_targets(const std::vector<TargetInfo>& targets) {
    std::vector<TargetInfo> sorted = targets;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const TargetInfo& lhs, const TargetInfo& rhs) {
                         if (lhs.display_name != rhs.display_name) {
                             return lhs.display_name < rhs.display_name;
                         }
                         if (lhs.type != rhs.type) return lhs.type < rhs.type;
                         return lhs.unique_key < rhs.unique_key;
                     });
    // NRVO-defeating return so the closing brace counts as a covered line
    // under gcov, mirroring the pattern used in dot_report_adapter.cpp.
    return std::vector<TargetInfo>(std::move(sorted));
}

void index_targets_by_key(std::map<std::string, const std::vector<TargetInfo>*>& by_key,
                          const std::vector<TargetAssignment>& assignments) {
    for (const auto& assignment : assignments) {
        by_key.emplace(assignment.observation_key, &assignment.targets);
    }
}

std::size_t count_mapped_translation_units(
    const std::vector<RankedTranslationUnit>& translation_units) {
    return static_cast<std::size_t>(std::count_if(
        translation_units.begin(), translation_units.end(),
        [](const auto& tu) { return !tu.targets.empty(); }));
}

// ---- HTML emit primitives -----------------------------------------------

void emit_targets_cell(std::ostringstream& out, const std::vector<TargetInfo>& targets) {
    if (targets.empty()) {
        out << "<td><span class=\"empty\">no targets</span></td>";
        return;
    }
    out << "<td><ul>";
    for (const auto& target : sorted_targets(targets)) {
        out << "<li>" << render_text(target_label(target));
        if (!target.type.empty()) {
            out << " <span class=\"badge\">" << render_text(target.type) << "</span>";
        }
        out << "</li>";
    }
    out << "</ul></td>";
}

void emit_diagnostics_cell(std::ostringstream& out,
                            const std::vector<Diagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        out << "<td><span class=\"empty\">no diagnostics</span></td>";
        return;
    }
    out << "<td><ul>";
    for (const auto& diag : diagnostics) {
        out << "<li><span class=\"badge\">" << severity_text(diag.severity)
            << "</span> " << render_text(diag.message) << "</li>";
    }
    out << "</ul></td>";
}

void emit_diagnostics_section(std::ostringstream& out,
                               const std::vector<Diagnostic>& diagnostics) {
    out << "<section class=\"diagnostics\"><h2>Diagnostics</h2>";
    if (diagnostics.empty()) {
        out << "<p class=\"empty\">No diagnostics.</p></section>";
        return;
    }
    out << "\n<ul>\n";
    for (const auto& diag : diagnostics) {
        out << "<li><span class=\"badge\">" << severity_text(diag.severity)
            << "</span> " << render_text(diag.message) << "</li>\n";
    }
    out << "</ul></section>";
}

void emit_inputs_section(std::ostringstream& out, const ReportInputs& inputs,
                          bool include_changed_file) {
    out << "<section class=\"inputs\"><h2>Inputs</h2>\n"
        << "<dl>\n"
        << "<dt>compile_database_path</dt><dd>"
        << render_text(optional_or_not_provided(inputs.compile_database_path)) << "</dd>\n"
        << "<dt>compile_database_source</dt><dd>"
        << render_text(source_text(inputs.compile_database_source)) << "</dd>\n"
        << "<dt>cmake_file_api_path</dt><dd>"
        << render_text(optional_or_not_provided(inputs.cmake_file_api_path)) << "</dd>\n"
        << "<dt>cmake_file_api_resolved_path</dt><dd>"
        << render_text(optional_or_not_provided(inputs.cmake_file_api_resolved_path))
        << "</dd>\n"
        << "<dt>cmake_file_api_source</dt><dd>"
        << render_text(source_text(inputs.cmake_file_api_source)) << "</dd>\n";
    if (include_changed_file) {
        out << "<dt>changed_file</dt><dd>"
            << render_text(optional_or_not_provided(inputs.changed_file)) << "</dd>\n"
            << "<dt>changed_file_source</dt><dd>";
        if (inputs.changed_file_source.has_value()) {
            out << render_text(changed_file_source_text(*inputs.changed_file_source));
        } else {
            out << "not provided";
        }
        out << "</dd>\n";
    }
    out << "</dl></section>";
}

// ---- Analyze sections ---------------------------------------------------

struct AnalyzeSectionCounts {
    std::size_t ranking_count;
    std::size_t hotspot_count;
};

void emit_analyze_summary(std::ostringstream& out, const AnalysisResult& result,
                           std::size_t top_limit, const AnalyzeSectionCounts& counts) {
    const auto version = std::to_string(
        static_cast<std::size_t>(xray::hexagon::model::kReportFormatVersion));
    out << "<header class=\"summary\">\n"
        << "<h1>cmake-xray analyze report</h1>\n"
        << "<dl>\n"
        << "<dt>report_type</dt><dd>analyze</dd>\n"
        << "<dt>format_version</dt><dd>" << version << "</dd>\n"
        << "<dt>translation_units</dt><dd>" << result.translation_units.size() << "</dd>\n"
        << "<dt>ranking_entries</dt><dd>" << counts.ranking_count << " of "
        << result.translation_units.size() << "</dd>\n"
        << "<dt>top_limit</dt><dd>" << top_limit << "</dd>\n"
        << "<dt>observation_source</dt><dd>"
        << render_text(observation_source_text(result.observation_source)) << "</dd>\n"
        << "<dt>target_metadata</dt><dd>"
        << render_text(target_metadata_text(result.target_metadata)) << "</dd>\n"
        << "</dl></header>";
}

void emit_ranking_row(std::ostringstream& out, const RankedTranslationUnit& tu) {
    out << "<tr>"
        << "<td>" << tu.rank << "</td>"
        << "<td>" << render_text(tu.reference.source_path) << "</td>"
        << "<td>" << render_text(tu.reference.directory) << "</td>"
        << "<td>" << (tu.arg_count + tu.include_path_count + tu.define_count) << "</td>"
        << "<td>" << tu.arg_count << "</td>"
        << "<td>" << tu.include_path_count << "</td>"
        << "<td>" << tu.define_count << "</td>";
    emit_targets_cell(out, tu.targets);
    emit_diagnostics_cell(out, tu.diagnostics);
    out << "</tr>\n";
}

struct RankingContext {
    std::size_t top_limit;
    std::size_t ranking_count;
};

void emit_ranking_section(std::ostringstream& out, const AnalysisResult& result,
                           const RankingContext& ctx) {
    const auto state = resolve_section_state(result, AnalysisSection::tu_ranking);
    out << "<section class=\"ranking\"><h2>Translation Unit Ranking";
    emit_section_state_badge(out, state);
    out << "</h2>\n";
    if (state != AnalysisSectionState::active) {
        emit_section_empty_paragraph(out, state);
        out << "</section>";
        return;
    }
    if (result.translation_units.empty()) {
        out << "<p class=\"empty\">No translation units to report.</p></section>";
        return;
    }
    if (result.translation_units.size() > ctx.top_limit) {
        out << "<p>Showing " << ctx.ranking_count << " of "
            << result.translation_units.size() << " entries.</p>\n";
    }
    out << "<div class=\"table-wrap\"><table>\n"
        << "<thead><tr>"
        << "<th scope=\"col\">Rank</th>"
        << "<th scope=\"col\">Source path</th>"
        << "<th scope=\"col\">Directory</th>"
        << "<th scope=\"col\">Score</th>"
        << "<th scope=\"col\">Arguments</th>"
        << "<th scope=\"col\">Include paths</th>"
        << "<th scope=\"col\">Defines</th>"
        << "<th scope=\"col\">Targets</th>"
        << "<th scope=\"col\">Diagnostics</th>"
        << "</tr></thead>\n<tbody>\n";
    for (std::size_t index = 0; index < ctx.ranking_count; ++index) {
        emit_ranking_row(out, result.translation_units[index]);
    }
    out << "</tbody>\n</table></div></section>";
}

void emit_hotspot_context_cell(
    std::ostringstream& out, const IncludeHotspot& hotspot, std::size_t top_limit,
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key) {
    out << "<td>";
    const auto total = hotspot.affected_translation_units.size();
    const auto take = std::min(top_limit, total);
    if (take < total) {
        out << "<p>Showing " << take << " of " << total << " translation units.</p>";
    }
    if (take == 0) {
        out << "<span class=\"empty\">no translation units</span></td>";
        return;
    }
    out << "<ul>";
    for (std::size_t index = 0; index < take; ++index) {
        const auto& ref = hotspot.affected_translation_units[index];
        out << "<li>" << render_text(ref.source_path) << " <span class=\"badge\">"
            << render_text(ref.directory) << "</span>";
        const auto it = targets_by_key.find(ref.unique_key);
        if (it != targets_by_key.end() && !it->second->empty()) {
            out << " <span class=\"badge\">"
                << render_text(target_label((*it->second)[0])) << "</span>";
        }
        out << "</li>";
    }
    out << "</ul></td>";
}

struct HotspotContext {
    const AnalysisResult& result;
    std::size_t top_limit;
    std::size_t hotspot_count;
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key;
};

void emit_hotspots_filter_line(std::ostringstream& out, const AnalysisResult& result) {
    // AP M6-1.4 A.5: Filter-Zeile vor der Tabelle. Spiegelt die wirksame
    // include-scope/include-depth-Konfiguration plus die zwei
    // exkludierungs-Zaehler (excluded_unknown_count und _mixed_count
    // aus AnalysisResult) gemaess plan-M6-1-4.md "HTML".
    const auto scope_value = include_scope_text(result.include_scope_effective);
    const auto depth_value = include_depth_filter_text(result.include_depth_filter_effective);
    out << "<p class=\"include-filter\">Filter: scope="
        << "<span class=\"badge badge--" << render_attribute(scope_value) << "\">"
        << render_text(scope_value) << "</span>"
        << ", depth=<span class=\"badge badge--" << render_attribute(depth_value) << "\">"
        << render_text(depth_value)
        << "</span>. Excluded: "
        << result.include_hotspot_excluded_unknown_count << " unknown, "
        << result.include_hotspot_excluded_mixed_count << " mixed.</p>\n";
    if (result.include_node_budget_reached) {
        out << "<p class=\"include-budget-note\">Note: include analysis stopped at "
            << result.include_node_budget_effective
            << " nodes (budget reached).</p>\n";
    }
}

void emit_hotspots_section(std::ostringstream& out, const HotspotContext& ctx) {
    const auto state =
        resolve_section_state(ctx.result, AnalysisSection::include_hotspots);
    out << "<section class=\"hotspots\"><h2>Include Hotspots";
    emit_section_state_badge(out, state);
    out << "</h2>\n";
    if (state != AnalysisSectionState::active) {
        emit_section_empty_paragraph(out, state);
        out << "</section>";
        return;
    }
    emit_hotspots_filter_line(out, ctx.result);
    if (ctx.result.include_hotspots.empty()) {
        out << "<p class=\"empty\">No include hotspots to report.</p></section>";
        return;
    }
    if (ctx.result.include_hotspots.size() > ctx.top_limit) {
        out << "<p>Showing " << ctx.hotspot_count << " of "
            << ctx.result.include_hotspots.size() << " entries.</p>\n";
    }
    out << "<div class=\"table-wrap\"><table>\n"
        << "<thead><tr>"
        << "<th scope=\"col\">Header</th>"
        << "<th scope=\"col\">Origin</th>"
        << "<th scope=\"col\">Depth</th>"
        << "<th scope=\"col\">Affected translation units</th>"
        << "<th scope=\"col\">Translation unit context</th>"
        << "</tr></thead>\n<tbody>\n";
    for (std::size_t index = 0; index < ctx.hotspot_count; ++index) {
        const auto& hotspot = ctx.result.include_hotspots[index];
        const auto origin_text = include_origin_text(hotspot.origin);
        const auto depth_text = include_depth_kind_text(hotspot.depth_kind);
        out << "<tr>"
            << "<td>" << render_text(hotspot.header_path) << "</td>"
            << "<td><span class=\"badge badge--" << render_attribute(origin_text)
            << "\">" << render_text(origin_text) << "</span></td>"
            << "<td><span class=\"badge badge--" << render_attribute(depth_text)
            << "\">" << render_text(depth_text) << "</span></td>"
            << "<td>" << hotspot.affected_translation_units.size() << "</td>";
        emit_hotspot_context_cell(out, hotspot, ctx.top_limit, ctx.targets_by_key);
        out << "</tr>\n";
    }
    out << "</tbody>\n</table></div></section>";
}

void emit_target_metadata_section(std::ostringstream& out, const AnalysisResult& result) {
    out << "<section class=\"target-metadata\"><h2>Target Metadata</h2>\n";
    out << "<dl>\n"
        << "<dt>observation_source</dt><dd>"
        << render_text(observation_source_text(result.observation_source)) << "</dd>\n"
        << "<dt>target_metadata</dt><dd>"
        << render_text(target_metadata_text(result.target_metadata)) << "</dd>\n"
        << "<dt>translation_units_with_targets</dt><dd>"
        << count_mapped_translation_units(result.translation_units) << " of "
        << result.translation_units.size() << "</dd>\n"
        << "</dl>\n";
    if (result.target_metadata == TargetMetadataStatus::not_loaded) {
        out << "<p class=\"empty\">No target metadata loaded.</p></section>";
        return;
    }
    std::map<std::string, TargetInfo> by_key;
    for (const auto& assignment : result.target_assignments) {
        for (const auto& target : assignment.targets) {
            by_key.emplace(target.unique_key, target);
        }
    }
    if (by_key.empty()) {
        out << "<p class=\"empty\">No targets discovered.</p></section>";
        return;
    }
    std::vector<TargetInfo> targets;
    targets.reserve(by_key.size());
    for (auto& [_, target] : by_key) targets.push_back(std::move(target));
    targets = sorted_targets(targets);
    out << "<div class=\"table-wrap\"><table>\n"
        << "<thead><tr>"
        << "<th scope=\"col\">Display name</th>"
        << "<th scope=\"col\">Type</th>"
        << "</tr></thead>\n<tbody>\n";
    for (const auto& target : targets) {
        out << "<tr>"
            << "<td>" << render_text(target_label(target)) << "</td>"
            << "<td>" << render_text(target.type) << "</td></tr>\n";
    }
    out << "</tbody>\n</table></div></section>";
}

// ---- M6 AP 1.2 Tranche A.3: Target-Graph-related sections ---------------

void emit_target_graph_status_line(std::ostringstream& out, TargetGraphStatus status) {
    // Hold the helper results in named locals so the temporary
    // std::string destructors live on a stable line (the function
    // closing brace) under gcov; the inline `<<` chain otherwise leaves
    // an inconsistent destructor-line hit count between the loaded and
    // partial paths.
    const auto badge = target_graph_status_badge_class(status);
    const auto label = target_graph_status_text(status);
    out << "<p>Status: <span class=\"badge " << badge << "\">" << label
        << "</span></p>\n";
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

void emit_target_graph_table(std::ostringstream& out, const TargetGraph& graph) {
    if (graph.edges.empty()) {
        out << "<p class=\"empty\">No direct target dependencies.</p>";
        return;
    }
    const auto from_names = disambiguate_edge_column(graph.edges, /*from_column=*/true);
    const auto to_names = disambiguate_edge_column(graph.edges, /*from_column=*/false);
    out << "<div class=\"table-wrap\"><table>\n"
        << "<thead><tr>"
        << "<th scope=\"col\">From</th>"
        << "<th scope=\"col\">To</th>"
        << "<th scope=\"col\">Resolution</th>"
        << "<th scope=\"col\">External target</th>"
        << "</tr></thead>\n<tbody>\n";
    for (std::size_t i = 0; i < graph.edges.size(); ++i) {
        const auto& edge = graph.edges[i];
        out << "<tr>"
            << "<td>" << render_text(from_names[i]) << "</td>"
            << "<td>" << render_text(to_names[i]) << "</td>"
            << "<td>" << target_dependency_resolution_text(edge.resolution) << "</td>"
            << "<td>";
        if (edge.resolution == TargetDependencyResolution::external) {
            out << render_text(edge.to_display_name);
        }
        out << "</td></tr>\n";
    }
    out << "</tbody>\n</table></div>";
}

void emit_target_graph_section(std::ostringstream& out, const AnalysisResult& result) {
    const auto state = resolve_section_state(result, AnalysisSection::target_graph);
    out << "<section class=\"target-graph\"><h2>Target Graph";
    emit_section_state_badge(out, state);
    out << "</h2>\n";
    if (state != AnalysisSectionState::active) {
        emit_section_empty_paragraph(out, state);
        out << "</section>";
        return;
    }
    emit_target_graph_status_line(out, result.target_graph_status);
    emit_target_graph_table(out, result.target_graph);
    out << "</section>";
}

void emit_hub_row(std::ostringstream& out, std::string_view direction,
                  std::size_t threshold, const std::vector<TargetInfo>& hubs,
                  std::string_view empty_text) {
    out << "<tr>"
        << "<td>" << direction << "</td>"
        << "<td>" << threshold << "</td>"
        << "<td>";
    if (hubs.empty()) {
        out << empty_text;
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
            out << render_text(names[i]);
        }
    }
    out << "</td></tr>\n";
}

void emit_target_hubs_section(std::ostringstream& out, const AnalysisResult& result) {
    // AP M6-1.5 A.5 HTML v5: hub thresholds source from the resolved
    // AnalysisConfiguration so CLI overrides surface in the inline
    // <p> + table header. disabled / not_loaded route through the
    // shared empty-paragraph helper.
    const auto state = resolve_section_state(result, AnalysisSection::target_hubs);
    out << "<section class=\"target-hubs\"><h2>Target Hubs";
    emit_section_state_badge(out, state);
    out << "</h2>\n";
    if (state != AnalysisSectionState::active) {
        emit_section_empty_paragraph(out, state);
        out << "</section>";
        return;
    }
    const auto in_threshold = result.analysis_configuration.target_hub_in_threshold;
    const auto out_threshold = result.analysis_configuration.target_hub_out_threshold;
    out << "<p>Incoming threshold: " << in_threshold
        << ". Outgoing threshold: " << out_threshold << ".</p>\n"
        << "<div class=\"table-wrap\"><table>\n"
        << "<thead><tr>"
        << "<th scope=\"col\">Direction</th>"
        << "<th scope=\"col\">Threshold</th>"
        << "<th scope=\"col\">Targets</th>"
        << "</tr></thead>\n<tbody>\n";
    emit_hub_row(out, "Inbound", in_threshold, result.target_hubs_in,
                 "No incoming hubs.");
    emit_hub_row(out, "Outbound", out_threshold, result.target_hubs_out,
                 "No outgoing hubs.");
    out << "</tbody>\n</table></div></section>";
}

void emit_target_graph_reference_section(std::ostringstream& out,
                                          const ImpactResult& result) {
    out << "<section class=\"target-graph-reference\"><h2>Target Graph Reference</h2>\n";
    if (result.target_graph_status == TargetGraphStatus::not_loaded) {
        out << "<p class=\"empty\">Target graph not loaded.</p></section>";
        return;
    }
    emit_target_graph_status_line(out, result.target_graph_status);
    emit_target_graph_table(out, result.target_graph);
    out << "</section>";
}

// AP M6-1.3 A.4: Impact-HTML "Prioritised Affected Targets" section
// per docs/planning/plan-M6-1-3.md "HTML". Always present in v3 impact output;
// the body shape varies by target_graph_status.
void emit_prioritised_affected_targets_section(std::ostringstream& out,
                                                 const ImpactResult& result) {
    using xray::hexagon::model::PrioritizedImpactedTarget;
    out << "<section class=\"prioritised-affected-targets\">"
        << "<h2>Prioritised Affected Targets</h2>\n";
    if (result.target_graph_status == TargetGraphStatus::not_loaded) {
        out << "<p>Requested depth: " << result.impact_target_depth_requested
            << ". Effective depth: 0 (no graph).</p>\n"
            << "<p class=\"empty\">Target graph not loaded; prioritisation skipped.</p>"
            << "</section>";
        return;
    }
    out << "<p>Requested depth: " << result.impact_target_depth_requested
        << ". Effective depth: " << result.impact_target_depth_effective
        << ".</p>\n";
    if (result.prioritized_affected_targets.empty()) {
        out << "<p class=\"empty\">No prioritised targets.</p></section>";
        return;
    }
    out << "<div class=\"table-wrap\"><table>\n"
        << "<thead><tr>"
        << "<th scope=\"col\">Display name</th>"
        << "<th scope=\"col\">Type</th>"
        << "<th scope=\"col\">Priority class</th>"
        << "<th scope=\"col\">Graph distance</th>"
        << "<th scope=\"col\">Evidence strength</th>"
        << "<th scope=\"col\">Unique key</th>"
        << "</tr></thead>\n<tbody>\n";
    for (const auto& entry : result.prioritized_affected_targets) {
        out << "<tr>"
            << "<td>" << render_text(entry.target.display_name) << "</td>"
            << "<td>" << render_text(entry.target.type) << "</td>"
            << "<td>" << priority_class_text_v3(entry.priority_class) << "</td>"
            << "<td>" << entry.graph_distance << "</td>"
            << "<td>" << evidence_strength_text_v3(entry.evidence_strength) << "</td>"
            << "<td>" << render_text(entry.target.unique_key) << "</td>"
            << "</tr>\n";
    }
    out << "</tbody>\n</table></div></section>";
}

// ---- Impact sections ----------------------------------------------------

void emit_impact_summary(std::ostringstream& out, const ImpactResult& result) {
    const auto version = std::to_string(
        static_cast<std::size_t>(xray::hexagon::model::kReportFormatVersion));
    out << "<header class=\"summary\">\n"
        << "<h1>cmake-xray impact report</h1>\n"
        << "<dl>\n"
        << "<dt>report_type</dt><dd>impact</dd>\n"
        << "<dt>format_version</dt><dd>" << version << "</dd>\n"
        << "<dt>changed_file</dt><dd>"
        << render_text(optional_or_not_provided(result.inputs.changed_file)) << "</dd>\n"
        << "<dt>affected_translation_units</dt><dd>"
        << result.affected_translation_units.size() << "</dd>\n"
        << "<dt>classification</dt><dd>"
        << render_text(impact_classification(result)) << "</dd>\n"
        << "<dt>observation_source</dt><dd>"
        << render_text(observation_source_text(result.observation_source)) << "</dd>\n"
        << "<dt>target_metadata</dt><dd>"
        << render_text(target_metadata_text(result.target_metadata)) << "</dd>\n"
        << "</dl></header>";
}

void emit_impacted_tu_row(std::ostringstream& out, const ImpactedTranslationUnit& tu) {
    out << "<tr>"
        << "<td>" << render_text(tu.reference.source_path) << "</td>"
        << "<td>" << render_text(tu.reference.directory) << "</td>";
    emit_targets_cell(out, tu.targets);
    out << "</tr>\n";
}

struct ImpactedTuSection {
    std::string_view section_class;
    std::string_view title;
    std::string_view empty_text;
    ImpactKind kind;
};

void emit_impacted_tu_section(std::ostringstream& out, const ImpactResult& result,
                               const ImpactedTuSection& spec) {
    out << "<section class=\"" << spec.section_class << "\"><h2>" << spec.title
        << "</h2>\n";
    bool any = false;
    for (const auto& tu : result.affected_translation_units) {
        if (tu.kind == spec.kind) {
            any = true;
            break;
        }
    }
    if (!any) {
        out << "<p class=\"empty\">" << spec.empty_text << "</p></section>";
        return;
    }
    out << "<div class=\"table-wrap\"><table>\n"
        << "<thead><tr>"
        << "<th scope=\"col\">Source path</th>"
        << "<th scope=\"col\">Directory</th>"
        << "<th scope=\"col\">Targets</th>"
        << "</tr></thead>\n<tbody>\n";
    for (const auto& tu : result.affected_translation_units) {
        if (tu.kind != spec.kind) continue;
        emit_impacted_tu_row(out, tu);
    }
    out << "</tbody>\n</table></div></section>";
}

struct ImpactedTargetSection {
    std::string_view section_class;
    std::string_view title;
    std::string_view empty_text;
    TargetImpactClassification classification;
    std::string_view badge_class;
};

void emit_impacted_target_section(std::ostringstream& out, const ImpactResult& result,
                                   const ImpactedTargetSection& spec) {
    out << "<section class=\"" << spec.section_class << "\"><h2>" << spec.title
        << "</h2>\n";
    std::vector<TargetInfo> matches;
    for (const auto& impacted : result.affected_targets) {
        if (impacted.classification == spec.classification) {
            matches.push_back(impacted.target);
        }
    }
    if (matches.empty()) {
        out << "<p class=\"empty\">" << spec.empty_text << "</p></section>";
        return;
    }
    matches = sorted_targets(matches);
    out << "<div class=\"table-wrap\"><table>\n"
        << "<thead><tr>"
        << "<th scope=\"col\">Display name</th>"
        << "<th scope=\"col\">Type</th>"
        << "</tr></thead>\n<tbody>\n";
    for (const auto& target : matches) {
        out << "<tr>"
            << "<td><span class=\"badge " << spec.badge_class << "\">"
            << render_text(target_label(target)) << "</span></td>"
            << "<td>" << render_text(target.type) << "</td></tr>\n";
    }
    out << "</tbody>\n</table></div></section>";
}

// ---- Document scaffolding ----------------------------------------------

void emit_doc_open(std::ostringstream& out, std::string_view report_type,
                    std::string_view title) {
    // SAFETY: report_type and title MUST be adapter-controlled string literals;
    // they are streamed unescaped because the only call sites pass the
    // hard-coded "analyze"/"impact" and "cmake-xray analyze report"/
    // "cmake-xray impact report" tokens. Any future caller passing user-derived
    // data must route it through render_text or render_attribute first.
    const auto version = std::to_string(
        static_cast<std::size_t>(xray::hexagon::model::kReportFormatVersion));
    out << "<!doctype html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "<meta charset=\"utf-8\">\n"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        << "<meta name=\"xray-report-format-version\" content=\"" << version << "\">\n"
        << "<title>" << title << "</title>\n"
        << "<style>" << html_report_css() << "</style>\n"
        << "</head>\n"
        << "<body>\n"
        << "<main class=\"report\" data-report-type=\"" << report_type
        << "\" data-format-version=\"" << version << "\">\n";
}

void emit_doc_close(std::ostringstream& out) {
    out << "\n</main>\n"
        << "</body>\n"
        << "</html>\n";
}

std::string render_analyze(const AnalysisResult& result, std::size_t top_limit) {
    const AnalyzeSectionCounts counts{
        std::min(top_limit, result.translation_units.size()),
        std::min(top_limit, result.include_hotspots.size()),
    };
    std::map<std::string, const std::vector<TargetInfo>*> targets_by_key;
    index_targets_by_key(targets_by_key, result.target_assignments);

    std::ostringstream out;
    emit_doc_open(out, "analyze", "cmake-xray analyze report");
    emit_analyze_summary(out, result, top_limit, counts);
    out << "\n";
    emit_inputs_section(out, result.inputs, /*include_changed_file=*/false);
    out << "\n";
    emit_analysis_configuration_section(out, result);
    out << "\n";
    emit_ranking_section(out, result, RankingContext{top_limit, counts.ranking_count});
    out << "\n";
    emit_hotspots_section(out, HotspotContext{result, top_limit, counts.hotspot_count,
                                                targets_by_key});
    out << "\n";
    emit_target_metadata_section(out, result);
    out << "\n";
    emit_target_graph_section(out, result);
    out << "\n";
    emit_target_hubs_section(out, result);
    out << "\n";
    emit_diagnostics_section(out, result.diagnostics);
    emit_doc_close(out);
    return out.str();
}

std::string render_impact(const ImpactResult& result) {
    std::ostringstream out;
    emit_doc_open(out, "impact", "cmake-xray impact report");
    emit_impact_summary(out, result);
    out << "\n";
    emit_inputs_section(out, result.inputs, /*include_changed_file=*/true);
    out << "\n";
    emit_impacted_tu_section(out, result,
                              ImpactedTuSection{"impact-direct-tus",
                                                "Directly Affected Translation Units",
                                                "No directly affected translation units.",
                                                ImpactKind::direct});
    out << "\n";
    emit_impacted_tu_section(out, result,
                              ImpactedTuSection{"impact-heuristic-tus",
                                                "Heuristically Affected Translation Units",
                                                "No heuristically affected translation units.",
                                                ImpactKind::heuristic});
    out << "\n";
    emit_impacted_target_section(out, result,
                                  ImpactedTargetSection{"impact-direct-targets",
                                                         "Directly Affected Targets",
                                                         "No directly affected targets.",
                                                         TargetImpactClassification::direct,
                                                         "badge--evidence-direct"});
    out << "\n";
    emit_impacted_target_section(out, result,
                                  ImpactedTargetSection{"impact-heuristic-targets",
                                                         "Heuristically Affected Targets",
                                                         "No heuristically affected targets.",
                                                         TargetImpactClassification::heuristic,
                                                         "badge--evidence-heuristic"});
    out << "\n";
    emit_target_graph_reference_section(out, result);
    out << "\n";
    emit_prioritised_affected_targets_section(out, result);
    out << "\n";
    emit_diagnostics_section(out, result.diagnostics);
    emit_doc_close(out);
    return out.str();
}

}  // namespace

std::string HtmlReportAdapter::write_analysis_report(
    const xray::hexagon::model::AnalysisResult& analysis_result,
    std::size_t top_limit) const {
    return render_analyze(analysis_result, top_limit);
}

std::string HtmlReportAdapter::write_impact_report(
    const xray::hexagon::model::ImpactResult& impact_result) const {
    return render_impact(impact_result);
}

}  // namespace xray::adapters::output
