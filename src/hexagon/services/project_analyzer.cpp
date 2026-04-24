#include "services/project_analyzer.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>

#include "model/application_info.h"
#include "model/diagnostic.h"
#include "services/analysis_support.h"
#include "services/diagnostic_support.h"

namespace xray::hexagon::services {

namespace {

struct LoadedInputs {
    std::filesystem::path base_directory;
    bool success{false};
};

LoadedInputs load_compile_commands_input(const ports::driven::BuildModelPort& port,
                                         std::string_view path,
                                         model::AnalysisResult& result) {
    const auto model = port.load_build_model(path);
    result.compile_database_path = display_compile_commands_path(path);
    result.compile_database = model.compile_database;
    result.observation_source = model::ObservationSource::exact;
    return {compile_commands_base_directory(path), model.is_success()};
}

LoadedInputs load_file_api_input(const ports::driven::BuildModelPort& port,
                                 std::string_view path,
                                 model::AnalysisResult& result) {
    const auto model = port.load_build_model(path);
    result.compile_database_path = display_compile_commands_path(path);
    result.compile_database = model.compile_database;
    result.observation_source = model::ObservationSource::derived;
    result.target_metadata = model.target_metadata;
    result.target_assignments = model.target_assignments;
    if (model.is_success()) {
        append_unique_diagnostics(result.diagnostics, model.diagnostics);
    }
    return {std::filesystem::path{model.source_root}, model.is_success()};
}

bool try_load_file_api(const ports::driven::BuildModelPort& port,
                       std::string_view path,
                       model::BuildModelResult& file_api_model,
                       model::AnalysisResult& result) {
    file_api_model = port.load_build_model(path);
    if (!file_api_model.is_success()) {
        result.compile_database = file_api_model.compile_database;
        return false;
    }
    append_unique_diagnostics(result.diagnostics, file_api_model.diagnostics);
    return true;
}

void apply_target_enrichment(
    const std::vector<model::TranslationUnitObservation>& observations,
    const model::BuildModelResult& file_api_model,
    model::AnalysisResult& result) {
    const auto& all_assignments = file_api_model.target_assignments;

    std::set<std::string> observation_keys;
    for (const auto& obs : observations) {
        observation_keys.insert(obs.reference.unique_key);
    }

    std::vector<model::TargetAssignment> matched;
    std::size_t unmatched_count = 0;
    for (const auto& assignment : all_assignments) {
        if (observation_keys.count(assignment.observation_key) > 0) {
            matched.push_back(assignment);
        } else {
            ++unmatched_count;
        }
    }

    result.target_metadata = file_api_model.target_metadata;
    result.target_assignments = std::move(matched);

    if (result.target_assignments.empty() && !all_assignments.empty()) {
        append_unique_diagnostic(
            result.diagnostics,
            {model::DiagnosticSeverity::warning,
             "no file api target assignment matches any compile database observation; "
             "check that both inputs describe the same project"});
    } else if (unmatched_count > 0) {
        append_unique_diagnostic(
            result.diagnostics,
            {model::DiagnosticSeverity::note,
             std::to_string(unmatched_count) + " of " +
                 std::to_string(all_assignments.size()) +
                 " file api observations have no match in the compile database"});
    }
}

void attach_targets_to_ranked_units(
    std::vector<model::RankedTranslationUnit>& translation_units,
    const std::vector<model::TargetAssignment>& assignments) {
    if (assignments.empty()) return;

    std::map<std::string, const std::vector<model::TargetInfo>*> targets_by_key;
    for (const auto& assignment : assignments) {
        targets_by_key.emplace(assignment.observation_key, &assignment.targets);
    }

    std::size_t mapped_count = 0;
    for (auto& tu : translation_units) {
        const auto it = targets_by_key.find(tu.reference.unique_key);
        if (it != targets_by_key.end()) {
            tu.targets = *it->second;
            ++mapped_count;
        }
    }

    // Partial target coverage is a reportwide diagnostic; individual TUs
    // carry only their own targets without redundant hints.
    (void)mapped_count;
}

void append_target_coverage_diagnostic(
    const std::vector<model::RankedTranslationUnit>& translation_units,
    const std::vector<model::TargetAssignment>& assignments,
    std::vector<model::Diagnostic>& diagnostics) {
    if (assignments.empty()) return;

    std::size_t mapped = 0;
    for (const auto& tu : translation_units) {
        if (!tu.targets.empty()) ++mapped;
    }

    if (mapped < translation_units.size()) {
        append_unique_diagnostic(
            diagnostics,
            {model::DiagnosticSeverity::note,
             std::to_string(mapped) + " of " +
                 std::to_string(translation_units.size()) +
                 " translation units have target assignments"});
    }
}

}  // namespace

ProjectAnalyzer::ProjectAnalyzer(
    const ports::driven::BuildModelPort& compile_db_port,
    const ports::driven::IncludeResolverPort& include_resolver_port,
    const ports::driven::BuildModelPort& file_api_port)
    : compile_db_port_(compile_db_port),
      file_api_port_(file_api_port),
      include_resolver_port_(include_resolver_port) {}

model::AnalysisResult ProjectAnalyzer::analyze_project(
    ports::driving::AnalyzeProjectRequest request) const {
    model::AnalysisResult result;
    result.application = model::application_info();

    const bool has_compile_commands = !request.compile_commands_path.empty();
    const bool has_file_api = !request.cmake_file_api_path.empty();
    LoadedInputs loaded;
    model::BuildModelResult file_api_model;
    if (has_compile_commands) {
        loaded = load_compile_commands_input(
            compile_db_port_, request.compile_commands_path, result);
        if (!loaded.success) return result;
        if (has_file_api &&
            !try_load_file_api(file_api_port_, request.cmake_file_api_path,
                               file_api_model, result)) {
            return result;
        }
    } else {
        loaded = load_file_api_input(file_api_port_, request.cmake_file_api_path, result);
        if (!loaded.success) return result;
    }

    const auto observations =
        build_translation_unit_observations(result.compile_database.entries(), loaded.base_directory);
    const auto include_resolution = include_resolver_port_.resolve_includes(observations);

    if (has_compile_commands && has_file_api) {
        apply_target_enrichment(observations, file_api_model, result);
    }

    result.include_analysis_heuristic = include_resolution.heuristic;
    result.translation_units = build_ranked_translation_units(observations, include_resolution);
    attach_targets_to_ranked_units(result.translation_units, result.target_assignments);
    result.include_hotspots =
        build_include_hotspots(observations, include_resolution, loaded.base_directory);
    append_unique_diagnostics(result.diagnostics, include_resolution.diagnostics);
    append_target_coverage_diagnostic(
        result.translation_units, result.target_assignments, result.diagnostics);

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
