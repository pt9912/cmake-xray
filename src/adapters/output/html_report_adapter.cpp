#include "adapters/output/html_report_adapter.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/target_info.h"
#include "hexagon/model/translation_unit.h"

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
    // Static, byte-stable CSS per docs/report-html.md "CSS-Vertrag".
    // Identical for analyze and impact. No @import / url(...) / external
    // fonts / animations / random values. System fontstacks only.
    static constexpr std::string_view kCss =
        "body{margin:0;padding:1.5rem;background:#ffffff;color:#1a1a1a;"
        "font-family:system-ui,-apple-system,\"Segoe UI\",Roboto,Helvetica,Arial,sans-serif;"
        "line-height:1.5;}"
        "main.report{max-width:80rem;margin:0 auto;}"
        "h1{font-size:1.75rem;margin:0 0 1rem 0;}"
        "h2{font-size:1.25rem;margin:1.5rem 0 0.75rem 0;}"
        "h3{font-size:1.0rem;margin:1rem 0 0.5rem 0;}"
        "section{margin-bottom:1.5rem;}"
        "table{border-collapse:collapse;width:100%;}"
        "th,td{border:1px solid #d0d0d0;padding:0.5rem;text-align:left;vertical-align:top;}"
        "th{background:#f3f3f3;font-weight:600;}"
        ".table-wrap{overflow-x:auto;}"
        ".badge{display:inline-block;padding:0.1rem 0.5rem;border:1px solid #1a1a1a;"
        "border-radius:0.25rem;background:#f0f0f0;color:#1a1a1a;font-size:0.875rem;}"
        ".badge-direct{background:#1a1a1a;color:#ffffff;}"
        ".badge-heuristic{background:#ffffff;color:#1a1a1a;}"
        ".empty{color:#404040;font-style:italic;}"
        "code,pre{font-family:ui-monospace,\"SFMono-Regular\",\"Menlo\",\"Consolas\",monospace;}"
        "@media print{body{background:#ffffff;color:#000000;}"
        "th{background:#ffffff;}}";
    return kCss;
}

namespace {

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
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetMetadataStatus;
using xray::hexagon::model::TranslationUnitReference;

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
    out << "<ul>";
    for (const auto& diag : diagnostics) {
        out << "<li><span class=\"badge\">" << severity_text(diag.severity)
            << "</span> " << render_text(diag.message) << "</li>";
    }
    out << "</ul></section>";
}

void emit_inputs_section(std::ostringstream& out, const ReportInputs& inputs,
                          bool include_changed_file) {
    out << "<section class=\"inputs\"><h2>Inputs</h2>"
        << "<dl>"
        << "<dt>compile_database_path</dt><dd>"
        << render_text(optional_or_not_provided(inputs.compile_database_path)) << "</dd>"
        << "<dt>compile_database_source</dt><dd>"
        << render_text(source_text(inputs.compile_database_source)) << "</dd>"
        << "<dt>cmake_file_api_path</dt><dd>"
        << render_text(optional_or_not_provided(inputs.cmake_file_api_path)) << "</dd>"
        << "<dt>cmake_file_api_resolved_path</dt><dd>"
        << render_text(optional_or_not_provided(inputs.cmake_file_api_resolved_path))
        << "</dd>"
        << "<dt>cmake_file_api_source</dt><dd>"
        << render_text(source_text(inputs.cmake_file_api_source)) << "</dd>";
    if (include_changed_file) {
        out << "<dt>changed_file</dt><dd>"
            << render_text(optional_or_not_provided(inputs.changed_file)) << "</dd>"
            << "<dt>changed_file_source</dt><dd>";
        if (inputs.changed_file_source.has_value()) {
            out << render_text(changed_file_source_text(*inputs.changed_file_source));
        } else {
            out << "not provided";
        }
        out << "</dd>";
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
    out << "<header class=\"summary\">"
        << "<h1>cmake-xray analyze report</h1>"
        << "<dl>"
        << "<dt>report_type</dt><dd>analyze</dd>"
        << "<dt>format_version</dt><dd>" << version << "</dd>"
        << "<dt>translation_units</dt><dd>" << result.translation_units.size() << "</dd>"
        << "<dt>ranking_entries</dt><dd>" << counts.ranking_count << " of "
        << result.translation_units.size() << "</dd>"
        << "<dt>top_limit</dt><dd>" << top_limit << "</dd>"
        << "<dt>observation_source</dt><dd>"
        << render_text(observation_source_text(result.observation_source)) << "</dd>"
        << "<dt>target_metadata</dt><dd>"
        << render_text(target_metadata_text(result.target_metadata)) << "</dd>"
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
    out << "</tr>";
}

struct RankingContext {
    std::size_t top_limit;
    std::size_t ranking_count;
};

void emit_ranking_section(std::ostringstream& out, const AnalysisResult& result,
                           const RankingContext& ctx) {
    out << "<section class=\"ranking\"><h2>Translation Unit Ranking</h2>";
    if (result.translation_units.empty()) {
        out << "<p class=\"empty\">No translation units to report.</p></section>";
        return;
    }
    if (result.translation_units.size() > ctx.top_limit) {
        out << "<p>Showing " << ctx.ranking_count << " of "
            << result.translation_units.size() << " entries.</p>";
    }
    out << "<div class=\"table-wrap\"><table><thead><tr>"
        << "<th scope=\"col\">Rank</th>"
        << "<th scope=\"col\">Source path</th>"
        << "<th scope=\"col\">Directory</th>"
        << "<th scope=\"col\">Score</th>"
        << "<th scope=\"col\">Arguments</th>"
        << "<th scope=\"col\">Include paths</th>"
        << "<th scope=\"col\">Defines</th>"
        << "<th scope=\"col\">Targets</th>"
        << "<th scope=\"col\">Diagnostics</th>"
        << "</tr></thead><tbody>";
    for (std::size_t index = 0; index < ctx.ranking_count; ++index) {
        emit_ranking_row(out, result.translation_units[index]);
    }
    out << "</tbody></table></div></section>";
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

void emit_hotspots_section(std::ostringstream& out, const HotspotContext& ctx) {
    out << "<section class=\"hotspots\"><h2>Include Hotspots</h2>";
    if (ctx.result.include_hotspots.empty()) {
        out << "<p class=\"empty\">No include hotspots to report.</p></section>";
        return;
    }
    if (ctx.result.include_hotspots.size() > ctx.top_limit) {
        out << "<p>Showing " << ctx.hotspot_count << " of "
            << ctx.result.include_hotspots.size() << " entries.</p>";
    }
    out << "<div class=\"table-wrap\"><table><thead><tr>"
        << "<th scope=\"col\">Header</th>"
        << "<th scope=\"col\">Affected translation units</th>"
        << "<th scope=\"col\">Translation unit context</th>"
        << "</tr></thead><tbody>";
    for (std::size_t index = 0; index < ctx.hotspot_count; ++index) {
        const auto& hotspot = ctx.result.include_hotspots[index];
        out << "<tr>"
            << "<td>" << render_text(hotspot.header_path) << "</td>"
            << "<td>" << hotspot.affected_translation_units.size() << "</td>";
        emit_hotspot_context_cell(out, hotspot, ctx.top_limit, ctx.targets_by_key);
        out << "</tr>";
    }
    out << "</tbody></table></div></section>";
}

void emit_target_metadata_section(std::ostringstream& out, const AnalysisResult& result) {
    out << "<section class=\"target-metadata\"><h2>Target Metadata</h2>";
    out << "<dl>"
        << "<dt>observation_source</dt><dd>"
        << render_text(observation_source_text(result.observation_source)) << "</dd>"
        << "<dt>target_metadata</dt><dd>"
        << render_text(target_metadata_text(result.target_metadata)) << "</dd>"
        << "<dt>translation_units_with_targets</dt><dd>"
        << count_mapped_translation_units(result.translation_units) << " of "
        << result.translation_units.size() << "</dd>"
        << "</dl>";
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
    out << "<div class=\"table-wrap\"><table><thead><tr>"
        << "<th scope=\"col\">Display name</th>"
        << "<th scope=\"col\">Type</th>"
        << "</tr></thead><tbody>";
    for (const auto& target : targets) {
        out << "<tr>"
            << "<td>" << render_text(target_label(target)) << "</td>"
            << "<td>" << render_text(target.type) << "</td></tr>";
    }
    out << "</tbody></table></div></section>";
}

// ---- Impact sections ----------------------------------------------------

void emit_impact_summary(std::ostringstream& out, const ImpactResult& result) {
    const auto version = std::to_string(
        static_cast<std::size_t>(xray::hexagon::model::kReportFormatVersion));
    out << "<header class=\"summary\">"
        << "<h1>cmake-xray impact report</h1>"
        << "<dl>"
        << "<dt>report_type</dt><dd>impact</dd>"
        << "<dt>format_version</dt><dd>" << version << "</dd>"
        << "<dt>changed_file</dt><dd>"
        << render_text(optional_or_not_provided(result.inputs.changed_file)) << "</dd>"
        << "<dt>affected_translation_units</dt><dd>"
        << result.affected_translation_units.size() << "</dd>"
        << "<dt>classification</dt><dd>"
        << render_text(impact_classification(result)) << "</dd>"
        << "<dt>observation_source</dt><dd>"
        << render_text(observation_source_text(result.observation_source)) << "</dd>"
        << "<dt>target_metadata</dt><dd>"
        << render_text(target_metadata_text(result.target_metadata)) << "</dd>"
        << "</dl></header>";
}

void emit_impacted_tu_row(std::ostringstream& out, const ImpactedTranslationUnit& tu) {
    out << "<tr>"
        << "<td>" << render_text(tu.reference.source_path) << "</td>"
        << "<td>" << render_text(tu.reference.directory) << "</td>";
    emit_targets_cell(out, tu.targets);
    out << "</tr>";
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
        << "</h2>";
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
    out << "<div class=\"table-wrap\"><table><thead><tr>"
        << "<th scope=\"col\">Source path</th>"
        << "<th scope=\"col\">Directory</th>"
        << "<th scope=\"col\">Targets</th>"
        << "</tr></thead><tbody>";
    for (const auto& tu : result.affected_translation_units) {
        if (tu.kind != spec.kind) continue;
        emit_impacted_tu_row(out, tu);
    }
    out << "</tbody></table></div></section>";
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
        << "</h2>";
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
    out << "<div class=\"table-wrap\"><table><thead><tr>"
        << "<th scope=\"col\">Display name</th>"
        << "<th scope=\"col\">Type</th>"
        << "</tr></thead><tbody>";
    for (const auto& target : matches) {
        out << "<tr>"
            << "<td><span class=\"badge " << spec.badge_class << "\">"
            << render_text(target_label(target)) << "</span></td>"
            << "<td>" << render_text(target.type) << "</td></tr>";
    }
    out << "</tbody></table></div></section>";
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
    emit_inputs_section(out, result.inputs, /*include_changed_file=*/false);
    emit_ranking_section(out, result, RankingContext{top_limit, counts.ranking_count});
    emit_hotspots_section(out, HotspotContext{result, top_limit, counts.hotspot_count,
                                                targets_by_key});
    emit_target_metadata_section(out, result);
    emit_diagnostics_section(out, result.diagnostics);
    emit_doc_close(out);
    return out.str();
}

std::string render_impact(const ImpactResult& result) {
    std::ostringstream out;
    emit_doc_open(out, "impact", "cmake-xray impact report");
    emit_impact_summary(out, result);
    emit_inputs_section(out, result.inputs, /*include_changed_file=*/true);
    emit_impacted_tu_section(out, result,
                              ImpactedTuSection{"impact-direct-tus",
                                                "Directly Affected Translation Units",
                                                "No directly affected translation units.",
                                                ImpactKind::direct});
    emit_impacted_tu_section(out, result,
                              ImpactedTuSection{"impact-heuristic-tus",
                                                "Heuristically Affected Translation Units",
                                                "No heuristically affected translation units.",
                                                ImpactKind::heuristic});
    emit_impacted_target_section(out, result,
                                  ImpactedTargetSection{"impact-direct-targets",
                                                         "Directly Affected Targets",
                                                         "No directly affected targets.",
                                                         TargetImpactClassification::direct,
                                                         "badge-direct"});
    emit_impacted_target_section(out, result,
                                  ImpactedTargetSection{"impact-heuristic-targets",
                                                         "Heuristically Affected Targets",
                                                         "No heuristically affected targets.",
                                                         TargetImpactClassification::heuristic,
                                                         "badge-heuristic"});
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
