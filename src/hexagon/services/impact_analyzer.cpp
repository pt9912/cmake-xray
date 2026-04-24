#include "services/impact_analyzer.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "model/application_info.h"
#include "model/diagnostic.h"
#include "services/analysis_support.h"
#include "services/diagnostic_support.h"

namespace xray::hexagon::services {

namespace {

using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactedTarget;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TranslationUnitObservation;

struct LoadedInputs {
    std::filesystem::path base_directory;
    bool success{false};
};

LoadedInputs load_compile_commands_input(const ports::driven::BuildModelPort& port,
                                         std::string_view path,
                                         ImpactResult& result) {
    const auto model = port.load_build_model(path);
    result.compile_database_path = display_compile_commands_path(path);
    result.compile_database = model.compile_database;
    result.observation_source = model::ObservationSource::exact;
    return {compile_commands_base_directory(path), model.is_success()};
}

LoadedInputs load_file_api_input(const ports::driven::BuildModelPort& port,
                                 std::string_view path,
                                 ImpactResult& result) {
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
                       ImpactResult& result) {
    file_api_model = port.load_build_model(path);
    if (!file_api_model.is_success()) {
        result.compile_database = file_api_model.compile_database;
        return false;
    }
    append_unique_diagnostics(result.diagnostics, file_api_model.diagnostics);
    return true;
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

ImpactResult ImpactAnalyzer::analyze_impact(
    std::string_view compile_commands_path,
    const std::filesystem::path& changed_path,
    std::string_view cmake_file_api_path) const {
    ImpactResult result;
    result.application = model::application_info();

    const bool has_compile_commands = !compile_commands_path.empty();
    const bool has_file_api = !cmake_file_api_path.empty();
    LoadedInputs loaded;
    model::BuildModelResult file_api_model;
    if (has_compile_commands) {
        loaded = load_compile_commands_input(compile_db_port_, compile_commands_path, result);
        if (!loaded.success) return result;
        if (has_file_api &&
            !try_load_file_api(file_api_port_, cmake_file_api_path, file_api_model, result)) {
            return result;
        }
    } else {
        loaded = load_file_api_input(file_api_port_, cmake_file_api_path, result);
        if (!loaded.success) return result;
    }

    const auto observations =
        build_translation_unit_observations(result.compile_database.entries(), loaded.base_directory);
    const auto include_resolution = include_resolver_port_.resolve_includes(observations);
    const auto observations_by_key = index_observations(observations);

    if (has_compile_commands && has_file_api) {
        filter_assignments_to_observations(observations_by_key, file_api_model, result);
    }

    result.changed_file_key = resolve_changed_file_key(loaded.base_directory, changed_path);
    result.changed_file = display_changed_file(loaded.base_directory, result.changed_file_key);
    collect_and_classify_impacts(observations, include_resolution, observations_by_key,
                                result.changed_file_key, result);
    normalize_report_diagnostics(result.diagnostics);
    return result;
}

}  // namespace xray::hexagon::services
