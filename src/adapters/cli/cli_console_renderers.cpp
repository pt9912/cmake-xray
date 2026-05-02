#include "adapters/cli/cli_console_renderers.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/report_inputs.h"
#include "hexagon/model/target_info.h"
#include "hexagon/model/translation_unit.h"

namespace xray::adapters::cli {

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

std::string_view target_metadata_text(TargetMetadataStatus status) {
    // The optional `target metadata: <status>` line is only emitted when the
    // status is not not_loaded, so this helper never sees not_loaded at
    // runtime. The default branch returns the documented label anyway so a
    // future caller cannot accidentally print an empty value.
    if (status == TargetMetadataStatus::partial) return "partial";
    return "loaded";
}

bool any_warning(const std::vector<Diagnostic>& diagnostics) {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
                       [](const Diagnostic& diag) {
                           return diag.severity == DiagnosticSeverity::warning;
                       });
}

bool analyze_has_any_warning(const AnalysisResult& result) {
    if (any_warning(result.diagnostics)) return true;
    return std::any_of(result.translation_units.begin(), result.translation_units.end(),
                       [](const auto& tu) { return any_warning(tu.diagnostics); });
}

bool impact_has_any_warning(const ImpactResult& result) {
    return any_warning(result.diagnostics);
}

}  // namespace

std::string render_console_quiet_analyze(const AnalysisResult& result, std::size_t top_limit) {
    std::ostringstream out;
    const auto tu_total = result.translation_units.size();
    const auto hotspot_total = result.include_hotspots.size();
    const auto ranking_shown = std::min(top_limit, tu_total);
    const auto hotspots_shown = std::min(top_limit, hotspot_total);

    out << "analysis: ok\n"
        << "translation units: " << tu_total << '\n'
        << "ranking entries shown: " << ranking_shown << " of " << tu_total << '\n'
        << "include hotspots shown: " << hotspots_shown << " of " << hotspot_total << '\n'
        << "diagnostics: " << result.diagnostics.size() << '\n';
    if (result.target_metadata != TargetMetadataStatus::not_loaded) {
        out << "target metadata: " << target_metadata_text(result.target_metadata) << '\n';
    }
    if (analyze_has_any_warning(result)) {
        out << "warnings: present\n";
    }
    return out.str();
}

std::string render_console_quiet_impact(const ImpactResult& result) {
    std::ostringstream out;
    // Render-Precondition guarantees inputs.changed_file is set when we get
    // here; cli_adapter rejects nullopt and unresolved_file_api_source_root
    // before invoking this renderer.
    const auto& changed_file = result.inputs.changed_file.value_or(std::string{});
    const auto* classification = result.heuristic ? "heuristic" : "direct";

    out << "impact: ok\n"
        << "changed file: " << changed_file << '\n'
        << "affected translation units: " << result.affected_translation_units.size() << '\n'
        << "classification: " << classification << '\n'
        << "affected targets: " << result.affected_targets.size() << '\n'
        << "diagnostics: " << result.diagnostics.size() << '\n';
    if (result.target_metadata != TargetMetadataStatus::not_loaded) {
        out << "target metadata: " << target_metadata_text(result.target_metadata) << '\n';
    }
    if (impact_has_any_warning(result)) {
        out << "warnings: present\n";
    }
    return out.str();
}

// ---- AP M5-1.5 Tranche B.2: Verbose render helpers ---------------------
//
// The verbose renderers split into one helper per pflichtsection so the top-
// level functions stay simple sequential composers. Each helper emits its
// own section heading line plus indented body lines and ends with a blank
// line separator; the top-level renderer terminates the document by trimming
// the trailing blank line so stdout ends with exactly one newline (single-
// newline contract from docs/planning/plan-M5-1-5.md).

namespace verbose_helpers {

constexpr std::size_t kHotspotContextDefaultLimit = 10;

std::string_view observation_source_text(ObservationSource source) {
    return source == ObservationSource::derived ? "derived" : "exact";
}

std::string_view target_metadata_full_text(TargetMetadataStatus status) {
    if (status == TargetMetadataStatus::loaded) return "loaded";
    if (status == TargetMetadataStatus::partial) return "partial";
    return "not_loaded";
}

std::string_view input_source_text(ReportInputSource source) {
    return source == ReportInputSource::cli ? "cli" : "not_provided";
}

std::string_view changed_file_source_text(ChangedFileSource source) {
    if (source == ChangedFileSource::compile_database_directory) {
        return "compile_database_directory";
    }
    if (source == ChangedFileSource::file_api_source_root) return "file_api_source_root";
    if (source == ChangedFileSource::cli_absolute) return "cli_absolute";
    return "unresolved_file_api_source_root";
}

std::string optional_or_not_provided(const std::optional<std::string>& value) {
    // NRVO-defeating return so gcov registers the exception-edge of the
    // implicit string destructor at every `<<` call site (mirrors the
    // pattern used in the JSON / HTML adapters).
    std::string out = value.has_value() ? *value : std::string{"not provided"};
    return std::string(std::move(out));
}

std::string normalize_diagnostic_message(std::string_view message) {
    // \n -> " / " mirrors the HTML adapter's whitespace contract; consecutive
    // CRLF/CR collapse to a single separator. \t and other control bytes
    // become a single space.
    std::string out;
    out.reserve(message.size());
    for (std::size_t index = 0; index < message.size(); ++index) {
        const auto ch = static_cast<unsigned char>(message[index]);
        if (ch == '\r') {
            const auto next = index + 1;
            if (next < message.size() && message[next] == '\n') ++index;
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
    // NRVO-defeating return mirrors the pattern in html_report_adapter and
    // dot_report_adapter so the closing brace counts as a covered line under
    // gcov.
    return std::string(std::move(out));
}

std::vector<TargetInfo> sort_targets_copy(const std::vector<TargetInfo>& targets) {
    std::vector<TargetInfo> sorted = targets;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const TargetInfo& lhs, const TargetInfo& rhs) {
                         if (lhs.display_name != rhs.display_name) {
                             return lhs.display_name < rhs.display_name;
                         }
                         if (lhs.type != rhs.type) return lhs.type < rhs.type;
                         return lhs.unique_key < rhs.unique_key;
                     });
    return std::vector<TargetInfo>(std::move(sorted));
}

void emit_target_inline(std::ostringstream& out, const std::vector<TargetInfo>& targets) {
    if (targets.empty()) return;
    const auto sorted = sort_targets_copy(targets);
    out << " [targets:";
    for (std::size_t index = 0; index < sorted.size(); ++index) {
        out << ' ' << sorted[index].display_name << " (" << sorted[index].type << ')';
        if (index + 1 < sorted.size()) out << ',';
    }
    out << ']';
}

void emit_inputs_section(std::ostringstream& out, const ReportInputs& inputs,
                          bool include_changed_file) {
    // Bind every optional-or-not-provided value to a local first; chaining
    // the function call directly into the `<<` stream confuses gcov's branch
    // coverage on the temporary's exception edge and shows up as a partially
    // uncovered line even when both has_value paths are exercised.
    const auto compile_db_path = optional_or_not_provided(inputs.compile_database_path);
    const auto file_api_path = optional_or_not_provided(inputs.cmake_file_api_path);
    const auto file_api_resolved =
        optional_or_not_provided(inputs.cmake_file_api_resolved_path);
    out << "inputs\n"
        << "  compile_database_path: " << compile_db_path << '\n'
        << "  compile_database_source: " << input_source_text(inputs.compile_database_source)
        << '\n'
        << "  cmake_file_api_path: " << file_api_path << '\n'
        << "  cmake_file_api_resolved_path: " << file_api_resolved << '\n'
        << "  cmake_file_api_source: " << input_source_text(inputs.cmake_file_api_source)
        << '\n';
    if (include_changed_file) {
        const auto changed_file = optional_or_not_provided(inputs.changed_file);
        out << "  changed_file: " << changed_file << '\n' << "  changed_file_source: ";
        if (inputs.changed_file_source.has_value()) {
            out << changed_file_source_text(*inputs.changed_file_source);
        } else {
            out << "not provided";
        }
        out << '\n';
    }
    out << '\n';
}

void emit_observation_analyze(std::ostringstream& out, const AnalysisResult& result) {
    out << "observation\n"
        << "  observation_source: " << observation_source_text(result.observation_source)
        << '\n'
        << "  target_metadata: " << target_metadata_full_text(result.target_metadata) << '\n'
        << "  include_analysis_heuristic: "
        << (result.include_analysis_heuristic ? "true" : "false") << '\n'
        << '\n';
}

void emit_observation_impact(std::ostringstream& out, const ImpactResult& result) {
    out << "observation\n"
        << "  observation_source: " << observation_source_text(result.observation_source)
        << '\n'
        << "  target_metadata: " << target_metadata_full_text(result.target_metadata) << '\n'
        << '\n';
}

void emit_diagnostics_block(std::ostringstream& out,
                             const std::vector<Diagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        out << "  no diagnostics\n";
        return;
    }
    for (const auto& diag : diagnostics) {
        const auto* severity =
            diag.severity == DiagnosticSeverity::warning ? "warning" : "note";
        out << "  " << severity << ": " << normalize_diagnostic_message(diag.message) << '\n';
    }
}

void emit_diagnostics_section(std::ostringstream& out,
                               const std::vector<Diagnostic>& diagnostics) {
    out << "diagnostics\n";
    emit_diagnostics_block(out, diagnostics);
    out << '\n';
}

std::vector<RankedTranslationUnit> sorted_ranking_copy(
    const std::vector<RankedTranslationUnit>& units) {
    std::vector<RankedTranslationUnit> sorted = units;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const RankedTranslationUnit& lhs, const RankedTranslationUnit& rhs) {
                         if (lhs.rank != rhs.rank) return lhs.rank < rhs.rank;
                         if (lhs.reference.source_path != rhs.reference.source_path) {
                             return lhs.reference.source_path < rhs.reference.source_path;
                         }
                         if (lhs.reference.directory != rhs.reference.directory) {
                             return lhs.reference.directory < rhs.reference.directory;
                         }
                         return lhs.reference.unique_key < rhs.reference.unique_key;
                     });
    return std::vector<RankedTranslationUnit>(std::move(sorted));
}

void emit_ranking_entry(std::ostringstream& out, const RankedTranslationUnit& tu) {
    out << "  " << tu.rank << ". " << tu.reference.source_path
        << " [directory: " << tu.reference.directory
        << "] [score: " << tu.score() << "]\n"
        << "    args=" << tu.arg_count
        << " includes=" << tu.include_path_count
        << " defines=" << tu.define_count;
    emit_target_inline(out, tu.targets);
    out << '\n';
    for (const auto& diag : tu.diagnostics) {
        const auto* severity =
            diag.severity == DiagnosticSeverity::warning ? "warning" : "note";
        out << "    " << severity << ": " << normalize_diagnostic_message(diag.message) << '\n';
    }
}

void emit_ranking_section(std::ostringstream& out, const AnalysisResult& result,
                           std::size_t top_limit) {
    out << "translation unit ranking\n";
    if (result.translation_units.empty()) {
        out << "  no translation units to report\n\n";
        return;
    }
    const auto sorted = sorted_ranking_copy(result.translation_units);
    const auto take = std::min(top_limit, sorted.size());
    out << "  showing " << take << " of " << sorted.size()
        << " (top_limit=" << top_limit << ")\n";
    for (std::size_t index = 0; index < take; ++index) {
        emit_ranking_entry(out, sorted[index]);
    }
    out << '\n';
}

std::vector<IncludeHotspot> sorted_hotspots_copy(const std::vector<IncludeHotspot>& hotspots) {
    std::vector<IncludeHotspot> sorted = hotspots;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const IncludeHotspot& lhs, const IncludeHotspot& rhs) {
                         const auto lhs_size = lhs.affected_translation_units.size();
                         const auto rhs_size = rhs.affected_translation_units.size();
                         if (lhs_size != rhs_size) return lhs_size > rhs_size;
                         return lhs.header_path < rhs.header_path;
                     });
    return std::vector<IncludeHotspot>(std::move(sorted));
}

std::vector<TranslationUnitReference> sorted_hotspot_context_copy(
    const std::vector<TranslationUnitReference>& refs) {
    std::vector<TranslationUnitReference> sorted = refs;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const TranslationUnitReference& lhs,
                        const TranslationUnitReference& rhs) {
                         if (lhs.source_path != rhs.source_path) {
                             return lhs.source_path < rhs.source_path;
                         }
                         if (lhs.directory != rhs.directory) {
                             return lhs.directory < rhs.directory;
                         }
                         return lhs.unique_key < rhs.unique_key;
                     });
    return std::vector<TranslationUnitReference>(std::move(sorted));
}

void emit_hotspot_entry(std::ostringstream& out, const IncludeHotspot& hotspot,
                         std::size_t context_limit) {
    out << "  - " << hotspot.header_path << " (affected translation units: "
        << hotspot.affected_translation_units.size() << ")\n";
    const auto sorted_refs = sorted_hotspot_context_copy(hotspot.affected_translation_units);
    const auto take = std::min(context_limit, sorted_refs.size());
    for (std::size_t index = 0; index < take; ++index) {
        out << "      " << sorted_refs[index].source_path << " [directory: "
            << sorted_refs[index].directory << "]\n";
    }
    if (take < sorted_refs.size()) {
        out << "      ... " << (sorted_refs.size() - take) << " more not shown\n";
    }
}

void emit_hotspots_section(std::ostringstream& out, const AnalysisResult& result,
                            std::size_t top_limit) {
    out << "include hotspots\n";
    if (result.include_hotspots.empty()) {
        out << "  no include hotspots\n\n";
        return;
    }
    const auto sorted = sorted_hotspots_copy(result.include_hotspots);
    const auto take = std::min(top_limit, sorted.size());
    out << "  showing " << take << " of " << sorted.size()
        << " (top_limit=" << top_limit << ")\n";
    const auto context_limit = std::min(top_limit, kHotspotContextDefaultLimit);
    for (std::size_t index = 0; index < take; ++index) {
        emit_hotspot_entry(out, sorted[index], context_limit);
    }
    out << '\n';
}

std::vector<TargetInfo> unique_targets_from_assignments(
    const std::vector<TargetAssignment>& assignments) {
    std::map<std::string, TargetInfo> by_key;
    for (const auto& assignment : assignments) {
        for (const auto& target : assignment.targets) {
            by_key.emplace(target.unique_key, target);
        }
    }
    std::vector<TargetInfo> targets;
    targets.reserve(by_key.size());
    for (auto& entry : by_key) targets.push_back(std::move(entry.second));
    return sort_targets_copy(targets);
}

void emit_targets_section(std::ostringstream& out, const AnalysisResult& result) {
    out << "targets\n";
    if (result.target_metadata == TargetMetadataStatus::not_loaded) {
        out << "  no target metadata loaded\n\n";
        return;
    }
    const auto targets = unique_targets_from_assignments(result.target_assignments);
    if (targets.empty()) {
        out << "  no targets discovered\n\n";
        return;
    }
    for (const auto& target : targets) {
        out << "  - " << target.display_name << " [type: " << target.type << "]\n";
    }
    out << '\n';
}

void emit_analyze_summary(std::ostringstream& out, const AnalysisResult& result,
                           std::size_t top_limit) {
    const auto tu_total = result.translation_units.size();
    const auto hotspot_total = result.include_hotspots.size();
    const auto ranking_shown = std::min(top_limit, tu_total);
    const auto hotspots_shown = std::min(top_limit, hotspot_total);
    out << "verbose analysis summary\n"
        << "  report_type: analyze\n"
        << "  format_version: "
        << static_cast<std::size_t>(xray::hexagon::model::kReportFormatVersion) << '\n'
        << "  translation_units: " << tu_total << '\n'
        << "  ranking_entries: " << ranking_shown << " of " << tu_total << '\n'
        << "  include_hotspots: " << hotspots_shown << " of " << hotspot_total << '\n'
        << "  top_limit: " << top_limit << '\n'
        << "  diagnostics: " << result.diagnostics.size() << '\n'
        << '\n';
}

std::vector<ImpactedTranslationUnit> sorted_impacted_tus(
    const std::vector<ImpactedTranslationUnit>& units, ImpactKind kind) {
    std::vector<ImpactedTranslationUnit> filtered;
    for (const auto& unit : units) {
        if (unit.kind == kind) filtered.push_back(unit);
    }
    std::stable_sort(filtered.begin(), filtered.end(),
                     [](const ImpactedTranslationUnit& lhs,
                        const ImpactedTranslationUnit& rhs) {
                         if (lhs.reference.source_path != rhs.reference.source_path) {
                             return lhs.reference.source_path < rhs.reference.source_path;
                         }
                         if (lhs.reference.directory != rhs.reference.directory) {
                             return lhs.reference.directory < rhs.reference.directory;
                         }
                         return lhs.reference.unique_key < rhs.reference.unique_key;
                     });
    return std::vector<ImpactedTranslationUnit>(std::move(filtered));
}

void emit_impacted_tus_section(std::ostringstream& out, std::string_view title,
                                const std::vector<ImpactedTranslationUnit>& units,
                                std::string_view empty_text) {
    out << title << '\n';
    if (units.empty()) {
        out << "  " << empty_text << "\n\n";
        return;
    }
    for (const auto& tu : units) {
        out << "  - " << tu.reference.source_path << " [directory: "
            << tu.reference.directory << ']';
        emit_target_inline(out, tu.targets);
        out << '\n';
    }
    out << '\n';
}

std::vector<TargetInfo> sorted_impacted_targets(
    const std::vector<ImpactedTarget>& impacts, TargetImpactClassification classification) {
    std::vector<TargetInfo> filtered;
    for (const auto& impact : impacts) {
        if (impact.classification == classification) filtered.push_back(impact.target);
    }
    return sort_targets_copy(filtered);
}

void emit_impacted_targets_section(std::ostringstream& out, std::string_view title,
                                    const std::vector<TargetInfo>& targets,
                                    std::string_view empty_text) {
    out << title << '\n';
    if (targets.empty()) {
        out << "  " << empty_text << "\n\n";
        return;
    }
    for (const auto& target : targets) {
        out << "  - " << target.display_name << " [type: " << target.type << "]\n";
    }
    out << '\n';
}

void emit_impact_summary(std::ostringstream& out, const ImpactResult& result) {
    const auto& changed_file = result.inputs.changed_file.value_or(std::string{});
    const auto* classification = result.heuristic ? "heuristic" : "direct";
    out << "verbose impact summary\n"
        << "  report_type: impact\n"
        << "  format_version: "
        << static_cast<std::size_t>(xray::hexagon::model::kReportFormatVersion) << '\n'
        << "  changed_file: " << changed_file << '\n'
        << "  affected_translation_units: " << result.affected_translation_units.size()
        << '\n'
        << "  classification: " << classification << '\n'
        << "  affected_targets: " << result.affected_targets.size() << '\n'
        << "  diagnostics: " << result.diagnostics.size() << '\n'
        << '\n';
}

}  // namespace verbose_helpers

std::string render_console_verbose_analyze(const AnalysisResult& result, std::size_t top_limit) {
    using namespace verbose_helpers;
    std::ostringstream out;
    emit_analyze_summary(out, result, top_limit);
    emit_inputs_section(out, result.inputs, /*include_changed_file=*/false);
    emit_observation_analyze(out, result);
    emit_ranking_section(out, result, top_limit);
    emit_hotspots_section(out, result, top_limit);
    emit_targets_section(out, result);
    emit_diagnostics_section(out, result.diagnostics);
    auto text = out.str();
    // Each section ends with "\n\n"; trim one to leave exactly one trailing
    // newline for the document, per docs/planning/plan-M5-1-5.md single-newline rule.
    if (!text.empty() && text.back() == '\n' && text.size() >= 2 &&
        text[text.size() - 2] == '\n') {
        text.pop_back();
    }
    return text;
}

std::string render_console_verbose_impact(const ImpactResult& result) {
    using namespace verbose_helpers;
    std::ostringstream out;
    emit_impact_summary(out, result);
    emit_inputs_section(out, result.inputs, /*include_changed_file=*/true);
    emit_observation_impact(out, result);
    emit_impacted_tus_section(out, "directly affected translation units",
                                sorted_impacted_tus(result.affected_translation_units,
                                                     ImpactKind::direct),
                                "no directly affected translation units");
    emit_impacted_tus_section(out, "heuristically affected translation units",
                                sorted_impacted_tus(result.affected_translation_units,
                                                     ImpactKind::heuristic),
                                "no heuristically affected translation units");
    emit_impacted_targets_section(
        out, "directly affected targets",
        sorted_impacted_targets(result.affected_targets,
                                TargetImpactClassification::direct),
        "no directly affected targets");
    emit_impacted_targets_section(
        out, "heuristically affected targets",
        sorted_impacted_targets(result.affected_targets,
                                TargetImpactClassification::heuristic),
        "no heuristically affected targets");
    emit_diagnostics_section(out, result.diagnostics);
    auto text = out.str();
    if (!text.empty() && text.back() == '\n' && text.size() >= 2 &&
        text[text.size() - 2] == '\n') {
        text.pop_back();
    }
    return text;
}

}  // namespace xray::adapters::cli
