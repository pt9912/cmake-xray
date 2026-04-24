#include "adapters/output/console_report_adapter.h"

#include <algorithm>
#include <map>
#include <sstream>
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

const std::vector<TargetInfo>* find_targets(
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key,
    const TranslationUnitReference& reference) {
    const auto it = targets_by_key.find(reference.unique_key);
    return it == targets_by_key.end() ? nullptr : it->second;
}

void append_translation_unit_reference(std::ostringstream& out,
                                       const TranslationUnitReference& reference) {
    out << reference.source_path << " [directory: " << reference.directory << ']';
}

void append_translation_unit_reference(
    std::ostringstream& out, const TranslationUnitReference& reference,
    const std::map<std::string, const std::vector<TargetInfo>*>& targets_by_key) {
    append_translation_unit_reference(out, reference);
    const auto* targets = find_targets(targets_by_key, reference);
    if (targets != nullptr) append_target_suffix(out, *targets);
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

void append_hotspot_section(std::ostringstream& out, const AnalysisResult& analysis_result,
                            std::size_t hotspot_count) {
    std::map<std::string, const std::vector<TargetInfo>*> targets_by_key;
    for (const auto& assignment : analysis_result.target_assignments) {
        targets_by_key.emplace(assignment.observation_key, &assignment.targets);
    }

    out << '\n';
    out << "include hotspots";
    if (analysis_result.include_analysis_heuristic) out << " [heuristic]";
    out << '\n';

    if (analysis_result.include_hotspots.empty()) {
        out << "no include hotspots found\n";
        return;
    }

    out << "top " << hotspot_count << " of " << analysis_result.include_hotspots.size()
        << " include hotspots\n";

    for (std::size_t index = 0; index < hotspot_count; ++index) {
        const auto& hotspot = analysis_result.include_hotspots[index];
        out << "- " << hotspot.header_path << " (affected translation units: "
            << hotspot.affected_translation_units.size() << ")\n";
        for (const auto& translation_unit : hotspot.affected_translation_units) {
            out << "  ";
            append_translation_unit_reference(out, translation_unit, targets_by_key);
            out << '\n';
        }
        append_diagnostics(out, hotspot.diagnostics, "  ");
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

}  // namespace

std::string ConsoleReportAdapter::write_analysis_report(const AnalysisResult& analysis_result,
                                                        std::size_t top_limit) const {
    std::ostringstream out;

    append_analysis_target_overview(out, analysis_result);
    const auto counts = append_ranking_section(out, analysis_result, top_limit);
    append_hotspot_section(out, analysis_result, counts.hotspot_count);
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

    append_diagnostics(out, impact_result.diagnostics, "");

    return out.str();
}

}  // namespace xray::adapters::output
