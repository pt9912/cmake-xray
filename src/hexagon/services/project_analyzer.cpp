#include "services/project_analyzer.h"

#include <algorithm>

#include "model/application_info.h"
#include "model/diagnostic.h"
#include "services/analysis_support.h"
#include "services/diagnostic_support.h"

namespace xray::hexagon::services {

namespace {

using xray::hexagon::model::AnalysisResult;

}  // namespace

ProjectAnalyzer::ProjectAnalyzer(
    const ports::driven::BuildModelPort& build_model_port,
    const ports::driven::IncludeResolverPort& include_resolver_port)
    : build_model_port_(build_model_port),
      include_resolver_port_(include_resolver_port) {}

model::AnalysisResult ProjectAnalyzer::analyze_project(
    std::string_view compile_commands_path) const {
    model::AnalysisResult result;
    result.application = model::application_info();
    result.compile_database_path = display_compile_commands_path(compile_commands_path);

    const auto build_model = build_model_port_.load_build_model(compile_commands_path);
    result.compile_database = build_model.compile_database;
    result.observation_source = build_model.source;
    result.target_metadata = build_model.target_metadata;
    result.target_assignments = build_model.target_assignments;

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
        append_unique_diagnostic(
            result.diagnostics,
            {model::DiagnosticSeverity::note,
             "include-based results are heuristic; conditional or generated includes may be missing"});
    }
    normalize_report_diagnostics(result.diagnostics);

    return result;
}

}  // namespace xray::hexagon::services
