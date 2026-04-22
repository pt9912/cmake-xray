#include "adapters/output/console_report_adapter.h"

#include <algorithm>
#include <sstream>

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

std::string diagnostic_prefix(DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::warning ? "warning" : "note";
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

}  // namespace

std::string ConsoleReportAdapter::write_analysis_report(const AnalysisResult& analysis_result,
                                                        std::size_t top_limit) const {
    std::ostringstream out;

    const auto ranking_count = std::min(top_limit, analysis_result.translation_units.size());
    out << "translation unit ranking\n";
    out << "based on metrics: arg_count + include_path_count + define_count\n";
    out << "top " << ranking_count << " of " << analysis_result.translation_units.size()
        << " translation units\n";

    for (std::size_t index = 0; index < ranking_count; ++index) {
        const auto& translation_unit = analysis_result.translation_units[index];
        out << translation_unit.rank << ". ";
        append_translation_unit_reference(out, translation_unit.reference);
        out << '\n';
        out << "   arg_count=" << translation_unit.arg_count
            << " include_path_count=" << translation_unit.include_path_count
            << " define_count=" << translation_unit.define_count << '\n';
        append_diagnostics(out, translation_unit.diagnostics, "   ");
    }

    out << '\n';
    out << "include hotspots";
    if (analysis_result.include_analysis_heuristic) out << " [heuristic]";
    out << '\n';

    const auto hotspot_count = std::min(top_limit, analysis_result.include_hotspots.size());
    if (analysis_result.include_hotspots.empty()) {
        out << "no include hotspots found\n";
    } else {
        out << "top " << hotspot_count << " of " << analysis_result.include_hotspots.size()
            << " include hotspots\n";

        for (std::size_t index = 0; index < hotspot_count; ++index) {
            const auto& hotspot = analysis_result.include_hotspots[index];
            out << "- " << hotspot.header_path << " (affected translation units: "
                << hotspot.affected_translation_units.size() << ")\n";
            for (const auto& translation_unit : hotspot.affected_translation_units) {
                out << "  ";
                append_translation_unit_reference(out, translation_unit);
                out << '\n';
            }
            append_diagnostics(out, hotspot.diagnostics, "  ");
        }
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

    for (const auto& impacted : impact_result.affected_translation_units) {
        out << "  ";
        append_translation_unit_reference(out, impacted.reference);
        out << " [" << (impacted.kind == ImpactKind::direct ? "direct" : "heuristic") << "]\n";
    }

    append_diagnostics(out, impact_result.diagnostics, "");

    return out.str();
}

}  // namespace xray::adapters::output
