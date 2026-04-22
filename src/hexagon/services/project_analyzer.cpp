#include "services/project_analyzer.h"

#include "model/application_info.h"
#include "model/diagnostic.h"
#include "services/analysis_support.h"

namespace xray::hexagon::services {

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

    if (result.include_analysis_heuristic) {
        result.diagnostics.push_back(
            {model::DiagnosticSeverity::note,
             "include-based results are heuristic; conditional or generated includes may be missing"});
    }

    return result;
}

}  // namespace xray::hexagon::services
