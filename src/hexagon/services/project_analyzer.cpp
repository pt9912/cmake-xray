#include "services/project_analyzer.h"

#include <algorithm>

#include "model/application_info.h"
#include "model/diagnostic.h"
#include "services/analysis_support.h"

namespace xray::hexagon::services {

namespace {

using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::Diagnostic;

bool diagnostics_equal(const Diagnostic& lhs, const Diagnostic& rhs) {
    return lhs.severity == rhs.severity && lhs.message == rhs.message;
}

void append_unique_diagnostic(std::vector<Diagnostic>& target, const Diagnostic& diagnostic) {
    const auto duplicate = std::any_of(target.begin(), target.end(), [&](const auto& existing) {
        return diagnostics_equal(existing, diagnostic);
    });
    if (!duplicate) target.push_back(diagnostic);
}

void append_unique_diagnostics(std::vector<Diagnostic>& target,
                               const std::vector<Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        append_unique_diagnostic(target, diagnostic);
    }
}

void append_translation_unit_diagnostics(AnalysisResult& result) {
    for (const auto& translation_unit : result.translation_units) {
        append_unique_diagnostics(result.diagnostics, translation_unit.diagnostics);
    }
}

}  // namespace

ProjectAnalyzer::ProjectAnalyzer(
    const ports::driven::CompileDatabasePort& compile_database_port,
    const ports::driven::IncludeResolverPort& include_resolver_port)
    : compile_database_port_(compile_database_port),
      include_resolver_port_(include_resolver_port) {}

model::AnalysisResult ProjectAnalyzer::analyze_project(
    std::string_view compile_commands_path) const {
    model::AnalysisResult result;
    result.application = model::application_info();
    result.compile_database = compile_database_port_.load_compile_database(compile_commands_path);

    if (!result.compile_database.is_success()) return result;

    const auto observations =
        build_translation_unit_observations(result.compile_database.entries(), compile_commands_path);
    const auto include_resolution = include_resolver_port_.resolve_includes(observations);

    result.include_analysis_heuristic = include_resolution.heuristic;
    result.translation_units = build_ranked_translation_units(observations, include_resolution);
    result.include_hotspots =
        build_include_hotspots(observations, include_resolution, compile_commands_path);
    result.diagnostics = include_resolution.diagnostics;
    append_translation_unit_diagnostics(result);

    if (result.include_analysis_heuristic) {
        append_unique_diagnostic(
            result.diagnostics,
            {model::DiagnosticSeverity::note,
             "include-based results are heuristic; conditional or generated includes may be missing"});
    }

    return result;
}

}  // namespace xray::hexagon::services
