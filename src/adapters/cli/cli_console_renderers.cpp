#include "adapters/cli/cli_console_renderers.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

#include "hexagon/model/diagnostic.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/observation_source.h"
#include "hexagon/model/translation_unit.h"

namespace xray::adapters::cli {

namespace {

using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::TargetMetadataStatus;

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

}  // namespace xray::adapters::cli
