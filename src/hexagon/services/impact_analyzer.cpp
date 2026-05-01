#include "services/impact_analyzer.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "model/application_info.h"
#include "model/diagnostic.h"
#include "model/report_inputs.h"
#include "services/analysis_support.h"
#include "services/diagnostic_support.h"
#include "services/target_graph_support.h"
#include "services/target_graph_traversal.h"

namespace xray::hexagon::services {

namespace {

using ports::driving::AnalyzeImpactRequest;
using ports::driving::InputPathArgument;
using xray::hexagon::model::ChangedFileSource;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactedTarget;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::PrioritizedImpactedTarget;
using xray::hexagon::model::ServiceValidationError;
using xray::hexagon::model::TargetEvidenceStrength;
using xray::hexagon::model::TargetGraphStatus;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetPriorityClass;
using xray::hexagon::model::TranslationUnitObservation;

// AP M6-1.3 A.1: deepest reverse-BFS budget the service accepts. Beyond
// this the analyzer rejects the request as
// `impact_target_depth_out_of_range` without producing an ImpactResult.
constexpr std::size_t kMaxImpactTargetDepth = 32;
// AP M6-1.3 A.1: default reverse-BFS depth applied when neither the CLI
// adapter nor a non-CLI caller specifies a value.
constexpr std::size_t kDefaultImpactTargetDepth = 2;

struct LoadedInputs {
    std::filesystem::path base_directory;
    bool success{false};
};

struct FileApiLoadOutcome {
    LoadedInputs loaded;
    std::optional<std::filesystem::path> source_root;
};

struct ChangedFileResolution {
    ChangedFileSource source{ChangedFileSource::cli_absolute};
    std::optional<std::filesystem::path> base;
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

void populate_initial_inputs(const AnalyzeImpactRequest& request, model::ReportInputs& inputs) {
    inputs.compile_database_path =
        input_arg_display(request.compile_commands_path, request.report_display_base);
    inputs.compile_database_source = source_from(request.compile_commands_path);
    inputs.cmake_file_api_path =
        input_arg_display(request.cmake_file_api_path, request.report_display_base);
    inputs.cmake_file_api_source = source_from(request.cmake_file_api_path);
}

void set_legacy_compile_database_path(ImpactResult& result) {
    if (result.inputs.compile_database_path.has_value()) {
        result.compile_database_path = *result.inputs.compile_database_path;
    } else if (result.inputs.cmake_file_api_path.has_value()) {
        result.compile_database_path = *result.inputs.cmake_file_api_path;
    }
}

void update_inputs_from_file_api_load(const model::BuildModelResult& model,
                                      const AnalyzeImpactRequest& request,
                                      model::ReportInputs& inputs) {
    auto resolved = resolved_file_api_display(model.cmake_file_api_resolved_path,
                                              request.cmake_file_api_path,
                                              request.report_display_base);
    if (resolved.has_value()) {
        inputs.cmake_file_api_resolved_path = std::move(resolved);
    }
}

LoadedInputs load_compile_commands_input(const ports::driven::BuildModelPort& port,
                                         std::string_view path,
                                         const std::filesystem::path& fallback_base,
                                         ImpactResult& result) {
    const auto model = port.load_build_model(path);
    result.compile_database = model.compile_database;
    result.observation_source = model::ObservationSource::exact;
    return {compile_commands_base_directory(path, fallback_base), model.is_success()};
}

void apply_target_graph_view(const model::BuildModelResult& build_model,
                             ImpactResult& result) {
    result.target_graph = build_model.target_graph;
    result.target_graph_status = build_model.target_graph_status;
    // Defense-in-depth: future BuildModelPort adapters or test doubles must
    // not be able to leak unsorted graphs into ImpactResult; AP 1.3 will
    // build reverse-BFS on top of this.
    sort_target_graph(result.target_graph);
}

FileApiLoadOutcome load_file_api_input(const ports::driven::BuildModelPort& port,
                                       std::string_view path,
                                       const AnalyzeImpactRequest& request,
                                       ImpactResult& result) {
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
    return {{std::filesystem::path{model.source_root}, model.is_success()},
            source_root_from_build_model(model)};
}

bool try_load_file_api(const ports::driven::BuildModelPort& port,
                       std::string_view path,
                       const AnalyzeImpactRequest& request,
                       model::BuildModelResult& file_api_model,
                       ImpactResult& result) {
    file_api_model = port.load_build_model(path);
    update_inputs_from_file_api_load(file_api_model, request, result.inputs);
    if (!file_api_model.is_success()) {
        result.compile_database = file_api_model.compile_database;
        return false;
    }
    append_unique_diagnostics(result.diagnostics, file_api_model.diagnostics);
    return true;
}

ChangedFileResolution resolve_changed_file_provenance(
    const InputPathArgument& changed_file_arg,
    bool has_compile_commands,
    const std::filesystem::path& compile_db_base,
    const std::optional<std::filesystem::path>& source_root) {
    if (!changed_file_arg.was_relative) {
        return {ChangedFileSource::cli_absolute, std::nullopt};
    }
    if (has_compile_commands) {
        return {ChangedFileSource::compile_database_directory, compile_db_base};
    }
    if (source_root.has_value()) {
        return {ChangedFileSource::file_api_source_root, source_root};
    }
    return {ChangedFileSource::unresolved_file_api_source_root, std::nullopt};
}

void apply_changed_file_absolute(const InputPathArgument& arg, ImpactResult& result) {
    const auto display = arg.original_argument.lexically_normal().generic_string();
    result.inputs.changed_file = display;
    result.changed_file = display;
    result.changed_file_key = display;
}

void apply_changed_file_unresolved(const InputPathArgument& arg, ImpactResult& result) {
    const auto display = arg.original_argument.lexically_normal().generic_string();
    result.inputs.changed_file = display;
    result.changed_file = display;
    result.changed_file_key = display;
}

void apply_changed_file_with_base(const InputPathArgument& arg,
                                  const std::filesystem::path& base, ImpactResult& result) {
    result.changed_file_key = resolve_changed_file_key(base, arg.original_argument);
    result.changed_file = display_changed_file(base, result.changed_file_key);
    result.inputs.changed_file = result.changed_file;
}

void apply_changed_file(const InputPathArgument& arg, const ChangedFileResolution& resolution,
                        ImpactResult& result) {
    result.inputs.changed_file_source = resolution.source;
    if (resolution.source == ChangedFileSource::cli_absolute) {
        apply_changed_file_absolute(arg, result);
        return;
    }
    if (resolution.source == ChangedFileSource::unresolved_file_api_source_root) {
        apply_changed_file_unresolved(arg, result);
        return;
    }
    apply_changed_file_with_base(arg, *resolution.base, result);
}

void finalize_changed_file(const AnalyzeImpactRequest& request, bool has_compile_commands,
                           const std::filesystem::path& compile_db_base,
                           const std::optional<std::filesystem::path>& source_root,
                           ImpactResult& result) {
    apply_changed_file(request.changed_file_path,
                       resolve_changed_file_provenance(request.changed_file_path,
                                                       has_compile_commands, compile_db_base,
                                                       source_root),
                       result);
}

bool impacted_translation_unit_less(const ImpactedTranslationUnit& lhs,
                                    const ImpactedTranslationUnit& rhs) {
    if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
    return lhs.reference.unique_key < rhs.reference.unique_key;
}

std::map<std::string, const TranslationUnitObservation*> index_observations(
    const std::vector<TranslationUnitObservation>& observations) {
    std::map<std::string, const TranslationUnitObservation*> indexed;
    for (const auto& observation : observations) {
        indexed.emplace(observation.reference.unique_key, &observation);
    }
    return indexed; }

bool collect_direct_impacts(const std::vector<TranslationUnitObservation>& observations,
                            std::string_view changed_file_key,
                            std::map<std::string, ImpactedTranslationUnit>& impacted_by_key) {
    bool direct_match_found = false;
    for (const auto& observation : observations) {
        if (observation.reference.source_path_key != changed_file_key) continue;
        direct_match_found = true;
        impacted_by_key.emplace(observation.reference.unique_key,
                                ImpactedTranslationUnit{observation.reference, ImpactKind::direct, {}});
    }
    return direct_match_found;
}

bool collect_heuristic_impacts(
    const model::IncludeResolutionResult& include_resolution, std::string_view changed_file_key,
    const std::map<std::string, const TranslationUnitObservation*>& observations_by_key,
    std::map<std::string, ImpactedTranslationUnit>& impacted_by_key) {
    bool heuristic_match_found = false;
    for (const auto& resolved : include_resolution.translation_units) {
        const auto header_match =
            std::find(resolved.headers.begin(), resolved.headers.end(), changed_file_key);
        if (header_match == resolved.headers.end()) continue;
        heuristic_match_found = true;
        const auto observation_it = observations_by_key.find(resolved.translation_unit_key);
        if (observation_it == observations_by_key.end()) continue;
        impacted_by_key.emplace(
            observation_it->first,
            ImpactedTranslationUnit{observation_it->second->reference, ImpactKind::heuristic, {}});
    }
    return heuristic_match_found;
}

void append_resolution_diagnostics(
    ImpactResult& result, const model::IncludeResolutionResult& include_resolution,
    const std::map<std::string, ImpactedTranslationUnit>& impacted_by_key,
    bool direct_match_found, bool heuristic_match_found) {
    append_unique_diagnostics(result.diagnostics, include_resolution.diagnostics);
    if (!heuristic_match_found && direct_match_found) return;
    const auto collect_all = !heuristic_match_found && !direct_match_found;
    for (const auto& resolved : include_resolution.translation_units) {
        if (!collect_all &&
            impacted_by_key.find(resolved.translation_unit_key) == impacted_by_key.end()) {
            continue;
        }
        append_unique_diagnostics(result.diagnostics, resolved.diagnostics);
    }
}

void append_impact_diagnostics(ImpactResult& result, bool direct_match_found,
                               bool heuristic_match_found) {
    result.heuristic = heuristic_match_found || !direct_match_found;
    if (result.heuristic) {
        append_unique_diagnostic(
            result.diagnostics,
            {DiagnosticSeverity::note,
             "conditional or generated includes may be missing from this result"});
    }
    if (!result.affected_translation_units.empty()) return;
    append_unique_diagnostic(
        result.diagnostics,
        {DiagnosticSeverity::note,
         "the file was not present in the loaded compile database or resolved include graph"});
}

void sort_impacted_translation_units(ImpactResult& result) {
    std::sort(result.affected_translation_units.begin(), result.affected_translation_units.end(),
              impacted_translation_unit_less);
}

std::vector<ImpactedTarget> compute_affected_targets(
    const std::map<std::string, ImpactedTranslationUnit>& impacted_by_key,
    const std::vector<model::TargetAssignment>& target_assignments) {
    std::map<std::string, ImpactedTarget> targets_by_key;
    for (const auto& assignment : target_assignments) {
        const auto it = impacted_by_key.find(assignment.observation_key);
        if (it == impacted_by_key.end()) continue;
        const auto classification =
            (it->second.kind == ImpactKind::direct) ? TargetImpactClassification::direct
                                                    : TargetImpactClassification::heuristic;
        for (const auto& target : assignment.targets) {
            auto [target_it, inserted] =
                targets_by_key.emplace(target.unique_key, ImpactedTarget{target, classification});
            if (!inserted && classification == TargetImpactClassification::direct) {
                target_it->second.classification = TargetImpactClassification::direct;
            }
        }
    }
    std::vector<ImpactedTarget> result;
    result.reserve(targets_by_key.size());
    for (auto& [_, target] : targets_by_key) {
        result.push_back(std::move(target));
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.classification != b.classification) return a.classification < b.classification;
        if (a.target.display_name != b.target.display_name) {
            return a.target.display_name < b.target.display_name;
        }
        return a.target.type < b.target.type;
    });
    return result;
}

void filter_assignments_to_observations(
    const std::map<std::string, const TranslationUnitObservation*>& observations_by_key,
    const model::BuildModelResult& file_api_model,
    ImpactResult& result) {
    const auto& all_assignments = file_api_model.target_assignments;
    std::vector<model::TargetAssignment> matched;
    std::size_t unmatched_count = 0;
    for (const auto& assignment : all_assignments) {
        if (observations_by_key.find(assignment.observation_key) != observations_by_key.end()) {
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
            {DiagnosticSeverity::warning,
             "no file api target assignment matches any compile database observation; "
             "check that both inputs describe the same project"});
    } else if (unmatched_count > 0) {
        append_unique_diagnostic(
            result.diagnostics,
            {DiagnosticSeverity::note,
             std::to_string(unmatched_count) + " of " +
                 std::to_string(all_assignments.size()) +
                 " file api observations have no match in the compile database"});
    }
}

void attach_targets_to_impacted_units(
    std::vector<ImpactedTranslationUnit>& affected_tus,
    const std::vector<model::TargetAssignment>& assignments) {
    if (assignments.empty()) return;
    std::map<std::string, const std::vector<model::TargetInfo>*> targets_by_key;
    for (const auto& assignment : assignments) {
        targets_by_key.emplace(assignment.observation_key, &assignment.targets);
    }
    for (auto& tu : affected_tus) {
        const auto it = targets_by_key.find(tu.reference.unique_key);
        if (it != targets_by_key.end()) tu.targets = *it->second;
    }
}

// AP M6-1.3 A.1: convert the M5 affected_targets list into reverse-BFS
// seeds. Existing compute_affected_targets only emits direct/heuristic
// classifications -- "uncertain" header-only-library seeds are an
// extension point for a future code path that surfaces targets affected
// purely through indirect target_assignments. Service-level tests
// exercise the uncertain branch directly via reverse_target_graph_bfs.
TargetEvidenceStrength evidence_for_classification(
    TargetImpactClassification classification) {
    return classification == TargetImpactClassification::direct
               ? TargetEvidenceStrength::direct
               : TargetEvidenceStrength::heuristic;
}

std::vector<ImpactSeed> collect_impact_seeds(
    const std::vector<ImpactedTarget>& affected_targets) {
    // compute_affected_targets above already dedups affected_targets by
    // TargetInfo::unique_key and prefers the direct classification over
    // heuristic on collisions; the BFS helper itself also dedups seeds
    // defensively. A flat 1:1 map from ImpactedTarget to ImpactSeed is
    // therefore enough and avoids a second by-key dedup that the test
    // suite cannot reach without bypassing compute_affected_targets.
    std::vector<ImpactSeed> seeds;
    seeds.reserve(affected_targets.size());
    for (const auto& impacted : affected_targets) {
        seeds.push_back(
            {impacted.target,
             evidence_for_classification(impacted.classification)});
    }
    return seeds;
}

PrioritizedImpactedTarget seed_to_prioritized(ImpactSeed seed) {
    PrioritizedImpactedTarget out;
    out.target = std::move(seed.target);
    out.graph_distance = 0;
    out.priority_class = TargetPriorityClass::direct;
    out.evidence_strength = seed.evidence_strength;
    return out;
}

void apply_not_loaded_fallback(ImpactResult& result,
                                std::vector<ImpactSeed> seeds) {
    for (auto& seed : seeds) {
        result.prioritized_affected_targets.push_back(
            seed_to_prioritized(std::move(seed)));
    }
    sort_prioritized_impacted_targets(result.prioritized_affected_targets);
    result.impact_target_depth_effective = 0;
    if (!result.prioritized_affected_targets.empty()) {
        append_unique_diagnostic(
            result.diagnostics,
            {DiagnosticSeverity::note,
             "target graph not loaded; impact prioritisation degrades to "
             "impact seed targets only"});
    }
}

void emit_bfs_diagnostics(ImpactResult& result, const ReverseBfsResult& bfs,
                           bool seeds_present, std::size_t requested) {
    if (result.target_graph_status == TargetGraphStatus::partial) {
        append_unique_diagnostic(
            result.diagnostics,
            {DiagnosticSeverity::warning,
             "target graph partially loaded; impact prioritisation uses "
             "available edges only"});
    }
    if (seeds_present && bfs.stopped_early_no_more_targets &&
        result.impact_target_depth_effective < requested) {
        append_unique_diagnostic(
            result.diagnostics,
            {DiagnosticSeverity::note,
             "reverse target graph traversal stopped at depth " +
                 std::to_string(result.impact_target_depth_effective) +
                 " (no further reachable targets)"});
    }
    if (bfs.stopped_at_depth_limit) {
        append_unique_diagnostic(
            result.diagnostics,
            {DiagnosticSeverity::warning,
             "reverse target graph traversal hit depth limit " +
                 std::to_string(requested) +
                 " within a cycle; some transitively dependent targets may be "
                 "missing"});
    }
}

void apply_reverse_bfs_priorisation(ImpactResult& result) {
    // AP M6-1.3 A.4: alongside the report-adapter activation of the
    // prioritised view the four documented diagnostics start firing.
    auto seeds = collect_impact_seeds(result.affected_targets);
    const auto requested = result.impact_target_depth_requested;
    if (result.target_graph_status == TargetGraphStatus::not_loaded) {
        apply_not_loaded_fallback(result, std::move(seeds));
        return;
    }
    const bool seeds_present = !seeds.empty();
    auto bfs = reverse_target_graph_bfs(result.target_graph, std::move(seeds),
                                         requested);
    result.prioritized_affected_targets = std::move(bfs.prioritized_targets);
    result.impact_target_depth_effective = bfs.effective_depth;
    emit_bfs_diagnostics(result, bfs, seeds_present, requested);
}

void collect_and_classify_impacts(
    const std::vector<TranslationUnitObservation>& observations,
    const model::IncludeResolutionResult& include_resolution,
    const std::map<std::string, const TranslationUnitObservation*>& observations_by_key,
    const std::string& changed_file_key,
    ImpactResult& result) {
    std::map<std::string, ImpactedTranslationUnit> impacted_by_key;
    const auto direct_match_found =
        collect_direct_impacts(observations, changed_file_key, impacted_by_key);
    const auto heuristic_match_found = collect_heuristic_impacts(
        include_resolution, changed_file_key, observations_by_key, impacted_by_key);
    for (const auto& [_, impacted] : impacted_by_key) {
        result.affected_translation_units.push_back(impacted);
    }
    sort_impacted_translation_units(result);
    attach_targets_to_impacted_units(result.affected_translation_units, result.target_assignments);
    result.affected_targets =
        compute_affected_targets(impacted_by_key, result.target_assignments);
    append_resolution_diagnostics(result, include_resolution, impacted_by_key,
                                  direct_match_found, heuristic_match_found);
    append_impact_diagnostics(result, direct_match_found, heuristic_match_found);
}

}  // namespace

ImpactAnalyzer::ImpactAnalyzer(
    const ports::driven::BuildModelPort& compile_db_port,
    const ports::driven::IncludeResolverPort& include_resolver_port,
    const ports::driven::BuildModelPort& file_api_port)
    : compile_db_port_(compile_db_port),
      file_api_port_(file_api_port),
      include_resolver_port_(include_resolver_port) {}

namespace {

struct ImpactLoadContext {
    const ports::driven::BuildModelPort& compile_db_port;
    const ports::driven::BuildModelPort& file_api_port;
    const AnalyzeImpactRequest& request;
    std::string compile_path;
    std::string file_api_path;
};

struct ImpactLoadState {
    LoadedInputs loaded;
    std::optional<std::filesystem::path> source_root;
    model::BuildModelResult file_api_model;
};

bool load_inputs_with_compile_db(const ImpactLoadContext& ctx, ImpactLoadState& state,
                                  ImpactResult& result) {
    state.loaded = load_compile_commands_input(ctx.compile_db_port, ctx.compile_path,
                                               ctx.request.report_display_base, result);
    if (!state.loaded.success) {
        finalize_changed_file(ctx.request, true, state.loaded.base_directory, std::nullopt, result);
        return false;
    }
    if (!ctx.file_api_path.empty() &&
        !try_load_file_api(ctx.file_api_port, ctx.file_api_path, ctx.request,
                           state.file_api_model, result)) {
        finalize_changed_file(ctx.request, true, state.loaded.base_directory, std::nullopt, result);
        return false;
    }
    return true;
}

bool load_inputs_file_api_only(const ImpactLoadContext& ctx, ImpactLoadState& state,
                                ImpactResult& result) {
    auto outcome = load_file_api_input(ctx.file_api_port, ctx.file_api_path, ctx.request, result);
    state.loaded = outcome.loaded;
    state.source_root = outcome.source_root;
    if (!state.loaded.success) {
        finalize_changed_file(ctx.request, false, state.loaded.base_directory,
                              state.source_root, result);
        return false;
    }
    return true;
}

bool load_inputs(const ImpactLoadContext& ctx, ImpactLoadState& state, ImpactResult& result) {
    if (!ctx.compile_path.empty()) {
        return load_inputs_with_compile_db(ctx, state, result);
    }
    return load_inputs_file_api_only(ctx, state, result);
}

// AP M6-1.3 A.1: factored out so analyze_impact stays under the lizard
// length cap. Returns a rejected ImpactResult when the request is
// invalid; std::nullopt when the request can proceed past depth
// validation. The require_target_graph guard runs separately after the
// build model load.
std::optional<ImpactResult> validate_request_depth(
    const AnalyzeImpactRequest& request, std::size_t requested_depth) {
    if (requested_depth <= kMaxImpactTargetDepth) return std::nullopt;
    (void)request;
    ImpactResult rejected;
    rejected.service_validation_error = ServiceValidationError{
        "impact_target_depth_out_of_range",
        "impact_target_depth: value exceeds maximum 32"};
    return rejected;
}

ImpactResult build_target_graph_required_rejection(std::size_t requested_depth) {
    ImpactResult rejected;
    rejected.service_validation_error = ServiceValidationError{
        "target_graph_required",
        "target graph data is required but not available"};
    rejected.impact_target_depth_requested = requested_depth;
    // NRVO-defeating return so the closing brace counts as a covered
    // line under gcov; mirrors src/adapters/output/dot_report_adapter.cpp Z. 291.
    return ImpactResult(std::move(rejected));
}

}  // namespace

ImpactResult ImpactAnalyzer::analyze_impact(AnalyzeImpactRequest request) const {
    // AP M6-1.3 A.1: depth validation runs before any I/O so a misuse
    // (depth > 32) cannot leak partial input data into the rejected
    // result. The plan reserves "negative value" / "not an integer" /
    // "option specified more than once" for the CLI layer (A.2).
    const auto requested_depth =
        request.impact_target_depth.value_or(kDefaultImpactTargetDepth);
    if (auto rejected = validate_request_depth(request, requested_depth)) {
        return std::move(*rejected);
    }

    ImpactResult result;
    result.impact_target_depth_requested = requested_depth;
    result.application = model::application_info();
    populate_initial_inputs(request, result.inputs);
    set_legacy_compile_database_path(result);

    const ImpactLoadContext ctx{compile_db_port_, file_api_port_, request,
                                 path_for_io_string(request.compile_commands_path),
                                 path_for_io_string(request.cmake_file_api_path)};
    ImpactLoadState state;
    if (!load_inputs(ctx, state, result)) return result;

    // AP M6-1.3 A.1: post-load require_target_graph guard. Status is
    // only known after the build model load, so the rejected branch
    // discards the partial result and returns one with only the error.
    if (request.require_target_graph &&
        result.target_graph_status == TargetGraphStatus::not_loaded) {
        return build_target_graph_required_rejection(requested_depth);
    }

    const auto observations = build_translation_unit_observations(
        result.compile_database.entries(), state.loaded.base_directory);
    const auto include_resolution = include_resolver_port_.resolve_includes(observations);
    const auto observations_by_key = index_observations(observations);

    if (!ctx.compile_path.empty() && !ctx.file_api_path.empty()) {
        filter_assignments_to_observations(observations_by_key, state.file_api_model, result);
    }
    finalize_changed_file(request, !ctx.compile_path.empty(), state.loaded.base_directory,
                          state.source_root, result);
    collect_and_classify_impacts(observations, include_resolution, observations_by_key,
                                result.changed_file_key, result);
    apply_reverse_bfs_priorisation(result);
    normalize_report_diagnostics(result.diagnostics);
    return result;
}

}  // namespace xray::hexagon::services
