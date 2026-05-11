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
    std::filesystem::path source_root;
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
                             const AnalyzeProjectRequest& request,
                             model::AnalysisResult& result) {
    result.target_graph = build_model.target_graph;
    result.target_graph_status = build_model.target_graph_status;
    // Defense-in-depth: BuildModelPort contract does not bind every adapter
    // to pre-sorted output; sort here so AnalysisResult goldens are stable.
    sort_target_graph(result.target_graph);
    // AP M6-1.5 A.2: hub thresholds come from the validated request fields
    // (CLI defaults 10/10 mirror the pre-AP-1.5 kDefaultTargetHub*Threshold
    // constants, which remain as fallback for unit tests that bypass the
    // request path).
    auto [in_hubs, out_hubs] = compute_target_hubs(
        result.target_graph,
        TargetHubThresholds{request.target_hub_in_threshold,
                            request.target_hub_out_threshold});
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
    return {compile_commands_base_directory(path, fallback_base), {}, model.is_success()};
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
    apply_target_graph_view(model, request, result);
    if (model.is_success()) {
        append_unique_diagnostics(result.diagnostics, model.diagnostics);
    }
    const auto source_root_path = std::filesystem::path{model.source_root};
    return {source_root_path, source_root_path, model.is_success()};
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
    const AnalyzeProjectRequest& request,
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
    apply_target_graph_view(file_api_model, request, result);

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
        if (ctx.file_api_path.empty()) return true;
        if (!try_load_file_api(ctx.file_api_port, ctx.file_api_path, ctx.request,
                               state.file_api_model, result)) return false;
        state.loaded.source_root = std::filesystem::path{state.file_api_model.source_root};
        return true;
    }
    state.loaded = load_file_api_only_input(ctx.file_api_port, ctx.file_api_path, ctx.request,
                                             result);
    return state.loaded.success;
}

void populate_analysis_configuration(const AnalyzeProjectRequest& request,
                                     model::AnalysisResult& result) {
    // AP M6-1.5 A.2: analysis_configuration mirrors the validated request
    // surface. requested_sections and effective_sections are identical in
    // AP 1.5 per plan §269-273; later APs may diverge them via auto-
    // augmentation or degradation. The four numeric fields and the
    // tu_thresholds map flow straight from the request defaults
    // (min_hotspot_tus=2, hub thresholds=10) when the user does not pass
    // the new flags.
    auto& cfg = result.analysis_configuration;
    cfg.requested_sections = request.analysis_sections;
    cfg.effective_sections = request.analysis_sections;
    cfg.tu_thresholds = request.tu_thresholds;
    cfg.min_hotspot_tus = request.min_hotspot_tus;
    cfg.target_hub_in_threshold = request.target_hub_in_threshold;
    cfg.target_hub_out_threshold = request.target_hub_out_threshold;
}

bool analysis_section_in(const std::vector<model::AnalysisSection>& sections,
                          model::AnalysisSection section) {
    return std::find(sections.begin(), sections.end(), section) != sections.end();
}

void populate_analysis_section_states(model::AnalysisResult& result) {
    // AP M6-1.5 A.2 plan §415-431: per-section state is disabled when the
    // section is not in effective_sections, not_loaded when requested but
    // the underlying data is missing, and active otherwise. tu-ranking
    // and include-hotspots are always data-available; target-graph and
    // target-hubs share availability and depend on target_graph_status.
    using model::AnalysisSection;
    using model::AnalysisSectionState;
    using model::TargetGraphStatus;
    const auto& effective = result.analysis_configuration.effective_sections;
    const auto state_for = [&effective](AnalysisSection section, bool data_available) {
        if (!analysis_section_in(effective, section)) return AnalysisSectionState::disabled;
        return data_available ? AnalysisSectionState::active : AnalysisSectionState::not_loaded;
    };
    const bool graph_available = result.target_graph_status == TargetGraphStatus::loaded ||
                                 result.target_graph_status == TargetGraphStatus::partial;
    result.analysis_section_states = {
        {AnalysisSection::tu_ranking, state_for(AnalysisSection::tu_ranking, true)},
        {AnalysisSection::include_hotspots,
         state_for(AnalysisSection::include_hotspots, true)},
        {AnalysisSection::target_graph,
         state_for(AnalysisSection::target_graph, graph_available)},
        {AnalysisSection::target_hubs,
         state_for(AnalysisSection::target_hubs, graph_available)},
    };
}

void populate_include_filter_telemetry(
    const model::IncludeResolutionResult& include_resolution,
    const services::IncludeHotspotsBuildResult& hotspots_build,
    const ports::driving::AnalyzeProjectRequest& request, model::AnalysisResult& result) {
    result.include_scope_requested = request.include_scope;
    result.include_scope_effective = request.include_scope;
    result.include_depth_filter_requested = request.include_depth;
    result.include_depth_filter_effective = request.include_depth;
    result.include_hotspot_total_count = hotspots_build.total_count;
    result.include_hotspot_returned_count = hotspots_build.hotspots.size();
    result.include_hotspot_excluded_unknown_count = hotspots_build.excluded_unknown_count;
    result.include_hotspot_excluded_mixed_count = hotspots_build.excluded_mixed_count;
    result.include_hotspot_excluded_by_min_tus_count = hotspots_build.excluded_by_min_tus_count;
    result.include_depth_limit_requested = include_resolution.include_depth_limit_requested;
    result.include_depth_limit_effective = include_resolution.include_depth_limit_effective;
    result.include_node_budget_requested = include_resolution.include_node_budget_requested;
    result.include_node_budget_effective = include_resolution.include_node_budget_effective;
    result.include_node_budget_reached = include_resolution.include_node_budget_reached;
}

void finalize_analysis(const ProjectLoadContext& ctx, const ProjectLoadState& state,
                       const ports::driven::IncludeResolverPort& include_resolver,
                       model::AnalysisResult& result) {
    const auto observations = build_translation_unit_observations(
        result.compile_database.entries(), state.loaded.base_directory);
    const auto include_resolution = include_resolver.resolve_includes(observations);

    if (!ctx.compile_path.empty() && !ctx.file_api_path.empty()) {
        apply_target_enrichment(observations, state.file_api_model, ctx.request, result);
    }

    result.include_analysis_heuristic = include_resolution.heuristic;
    auto ranked_build = build_ranked_translation_units(observations, include_resolution,
                                                       ctx.request.tu_thresholds);
    result.translation_units = std::move(ranked_build.translation_units);
    result.tu_ranking_total_count_after_thresholds =
        ranked_build.total_count_after_thresholds;
    result.tu_ranking_excluded_by_thresholds_count =
        ranked_build.excluded_by_thresholds_count;
    attach_targets_to_ranked_units(result.translation_units, result.target_assignments);
    auto hotspots_build = build_include_hotspots(
        observations, include_resolution, state.loaded.base_directory, state.loaded.source_root,
        services::IncludeHotspotFilters{ctx.request.include_scope, ctx.request.include_depth,
                                         ctx.request.min_hotspot_tus});
    populate_include_filter_telemetry(include_resolution, hotspots_build, ctx.request, result);
    result.include_hotspots = std::move(hotspots_build.hotspots);
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
    result.include_scope_requested = request.include_scope;
    result.include_scope_effective = request.include_scope;
    result.include_depth_filter_requested = request.include_depth;
    result.include_depth_filter_effective = request.include_depth;
    populate_analysis_configuration(request, result);

    const ProjectLoadContext ctx{compile_db_port_, file_api_port_, request,
                                  path_for_io_string(request.compile_commands_path),
                                  path_for_io_string(request.cmake_file_api_path)};
    ProjectLoadState state;
    if (load_project_inputs(ctx, state, result)) {
        finalize_analysis(ctx, state, include_resolver_port_, result);
    }
    populate_analysis_section_states(result);
    return result;
}

}  // namespace xray::hexagon::services
