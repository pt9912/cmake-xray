#include "services/project_analyzer.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "model/application_info.h"
#include "model/diagnostic.h"
#include "model/report_inputs.h"
#include "services/analysis_support.h"
#include "services/diagnostic_support.h"
#include "services/target_graph_support.h"

namespace xray::hexagon::services {

namespace {

using ports::driving::AnalyzeProjectRequest;
using ports::driving::InputPathArgument;

struct LoadedInputs {
    std::filesystem::path base_directory;
    bool success{false};
};

std::string path_for_io_string(const std::optional<InputPathArgument>& opt) {
    return opt.has_value() ? opt->path_for_io.generic_string() : std::string{};
}

model::ReportInputSource source_from(const std::optional<InputPathArgument>& opt) {
    return opt.has_value() ? model::ReportInputSource::cli
                           : model::ReportInputSource::not_provided;
}

std::optional<std::string> input_arg_display(const std::optional<InputPathArgument>& opt,
                                             const std::filesystem::path& base) {
    if (!opt.has_value()) return std::nullopt;
    return to_report_display_path(
        {opt->original_argument, ReportPathDisplayKind::input_argument, opt->was_relative},
        base);
}

std::optional<std::string> resolved_file_api_display(
    const std::optional<std::filesystem::path>& adapter_path,
    const std::optional<InputPathArgument>& input_arg,
    const std::filesystem::path& base) {
    if (!adapter_path.has_value()) return std::nullopt;
    const bool was_relative = input_arg.has_value() && input_arg->was_relative;
    return to_report_display_path(
        {adapter_path, ReportPathDisplayKind::resolved_adapter_path, was_relative}, base);
}

void populate_initial_inputs(const AnalyzeProjectRequest& request, model::ReportInputs& inputs) {
    inputs.compile_database_path =
        input_arg_display(request.compile_commands_path, request.report_display_base);
    inputs.compile_database_source = source_from(request.compile_commands_path);
    inputs.cmake_file_api_path =
        input_arg_display(request.cmake_file_api_path, request.report_display_base);
    inputs.cmake_file_api_source = source_from(request.cmake_file_api_path);
}

void set_legacy_compile_database_path(model::AnalysisResult& result) {
    if (result.inputs.compile_database_path.has_value()) {
        result.compile_database_path = *result.inputs.compile_database_path;
    } else if (result.inputs.cmake_file_api_path.has_value()) {
        result.compile_database_path = *result.inputs.cmake_file_api_path;
    }
}

void update_inputs_from_file_api_load(const model::BuildModelResult& model,
                                      const AnalyzeProjectRequest& request,
                                      model::ReportInputs& inputs) {
    auto resolved = resolved_file_api_display(model.cmake_file_api_resolved_path,
                                              request.cmake_file_api_path,
                                              request.report_display_base);
    if (resolved.has_value()) {
        inputs.cmake_file_api_resolved_path = std::move(resolved);
    }
}

void apply_target_graph_view(const model::BuildModelResult& build_model,
                             model::AnalysisResult& result) {
    result.target_graph = build_model.target_graph;
    result.target_graph_status = build_model.target_graph_status;
    // Defense-in-depth: BuildModelPort contract does not bind every adapter
    // to pre-sorted output; sort here so AnalysisResult goldens are stable.
    sort_target_graph(result.target_graph);
    auto [in_hubs, out_hubs] = compute_target_hubs(
        result.target_graph,
        TargetHubThresholds{kDefaultTargetHubInThreshold,
                            kDefaultTargetHubOutThreshold});
    result.target_hubs_in = std::move(in_hubs);
    result.target_hubs_out = std::move(out_hubs);
}

LoadedInputs load_compile_commands_input(const ports::driven::BuildModelPort& port,
                                         std::string_view path,
                                         const std::filesystem::path& fallback_base,
                                         model::AnalysisResult& result) {
    const auto model = port.load_build_model(path);
    result.compile_database = model.compile_database;
    result.observation_source = model::ObservationSource::exact;
    return {compile_commands_base_directory(path, fallback_base), model.is_success()};
}

LoadedInputs load_file_api_only_input(const ports::driven::BuildModelPort& port,
                                      std::string_view path,
                                      const AnalyzeProjectRequest& request,
                                      model::AnalysisResult& result) {
    const auto model = port.load_build_model(path);
    update_inputs_from_file_api_load(model, request, result.inputs);
    result.compile_database = model.compile_database;
    result.observation_source = model::ObservationSource::derived;
    result.target_metadata = model.target_metadata;
    result.target_assignments = model.target_assignments;
    apply_target_graph_view(model, result);
    if (model.is_success()) {
        append_unique_diagnostics(result.diagnostics, model.diagnostics);
    }
    return {std::filesystem::path{model.source_root}, model.is_success()};
}

bool try_load_file_api(const ports::driven::BuildModelPort& port,
                       std::string_view path,
                       const AnalyzeProjectRequest& request,
                       model::BuildModelResult& file_api_model,
                       model::AnalysisResult& result) {
    file_api_model = port.load_build_model(path);
    update_inputs_from_file_api_load(file_api_model, request, result.inputs);
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
    apply_target_graph_view(file_api_model, result);

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

    for (auto& tu : translation_units) {
        const auto it = targets_by_key.find(tu.reference.unique_key);
        if (it != targets_by_key.end()) {
            tu.targets = *it->second;
        }
    }
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

namespace {

struct ProjectLoadContext {
    const ports::driven::BuildModelPort& compile_db_port;
    const ports::driven::BuildModelPort& file_api_port;
    const AnalyzeProjectRequest& request;
    std::string compile_path;
    std::string file_api_path;
};

struct ProjectLoadState {
    LoadedInputs loaded;
    model::BuildModelResult file_api_model;
};

bool load_project_inputs(const ProjectLoadContext& ctx, ProjectLoadState& state,
                          model::AnalysisResult& result) {
    if (!ctx.compile_path.empty()) {
        state.loaded = load_compile_commands_input(ctx.compile_db_port, ctx.compile_path,
                                                   ctx.request.report_display_base, result);
        if (!state.loaded.success) return false;
        return ctx.file_api_path.empty() ||
               try_load_file_api(ctx.file_api_port, ctx.file_api_path, ctx.request,
                                 state.file_api_model, result);
    }
    state.loaded = load_file_api_only_input(ctx.file_api_port, ctx.file_api_path, ctx.request,
                                             result);
    return state.loaded.success;
}

void finalize_analysis(const ProjectLoadContext& ctx, const ProjectLoadState& state,
                       const ports::driven::IncludeResolverPort& include_resolver,
                       model::AnalysisResult& result) {
    const auto observations = build_translation_unit_observations(
        result.compile_database.entries(), state.loaded.base_directory);
    const auto include_resolution = include_resolver.resolve_includes(observations);

    if (!ctx.compile_path.empty() && !ctx.file_api_path.empty()) {
        apply_target_enrichment(observations, state.file_api_model, result);
    }

    result.include_analysis_heuristic = include_resolution.heuristic;
    result.translation_units = build_ranked_translation_units(observations, include_resolution);
    attach_targets_to_ranked_units(result.translation_units, result.target_assignments);
    result.include_hotspots =
        build_include_hotspots(observations, include_resolution, state.loaded.base_directory);
    append_unique_diagnostics(result.diagnostics, include_resolution.diagnostics);
    append_target_coverage_diagnostic(result.translation_units, result.target_assignments,
                                      result.diagnostics);

    if (result.include_analysis_heuristic) {
        append_unique_diagnostic(
            result.diagnostics,
            {model::DiagnosticSeverity::note,
             "include-based results are heuristic; conditional or generated includes may be missing"});
    }
    normalize_report_diagnostics(result.diagnostics);
}

}  // namespace

model::AnalysisResult ProjectAnalyzer::analyze_project(AnalyzeProjectRequest request) const {
    model::AnalysisResult result;
    result.application = model::application_info();
    populate_initial_inputs(request, result.inputs);
    set_legacy_compile_database_path(result);

    const ProjectLoadContext ctx{compile_db_port_, file_api_port_, request,
                                  path_for_io_string(request.compile_commands_path),
                                  path_for_io_string(request.cmake_file_api_path)};
    ProjectLoadState state;
    if (!load_project_inputs(ctx, state, result)) return result;
    finalize_analysis(ctx, state, include_resolver_port_, result);
    return result;
}

}  // namespace xray::hexagon::services
