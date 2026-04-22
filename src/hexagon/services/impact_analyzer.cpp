#include "services/impact_analyzer.h"

#include <algorithm>
#include <map>
#include <utility>

#include "model/application_info.h"
#include "model/diagnostic.h"
#include "services/analysis_support.h"

namespace xray::hexagon::services {

namespace {

using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::TranslationUnitObservation;

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
                                ImpactedTranslationUnit{observation.reference, ImpactKind::direct});
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
            ImpactedTranslationUnit{observation_it->second->reference, ImpactKind::heuristic});
    }

    return heuristic_match_found;
}

void append_resolution_diagnostics(
    ImpactResult& result, const model::IncludeResolutionResult& include_resolution,
    const std::map<std::string, ImpactedTranslationUnit>& impacted_by_key,
    bool direct_match_found, bool heuristic_match_found) {
    append_unique_diagnostics(result.diagnostics, include_resolution.diagnostics);

    if (!heuristic_match_found && direct_match_found) return;

    const auto collect_all_translation_unit_diagnostics =
        !heuristic_match_found && !direct_match_found;

    for (const auto& resolved : include_resolution.translation_units) {
        if (!collect_all_translation_unit_diagnostics &&
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

}  // namespace

ImpactAnalyzer::ImpactAnalyzer(
    const ports::driven::CompileDatabasePort& compile_database_port,
    const ports::driven::IncludeResolverPort& include_resolver_port)
    : compile_database_port_(compile_database_port),
      include_resolver_port_(include_resolver_port) {}

ImpactResult ImpactAnalyzer::analyze_impact(std::string_view compile_commands_path,
                                            const std::filesystem::path& changed_path) const {
    ImpactResult result;
    result.application = model::application_info();
    result.compile_database = compile_database_port_.load_compile_database(compile_commands_path);

    if (!result.compile_database.is_success()) return result;

    const auto base_directory = compile_commands_base_directory(compile_commands_path);
    const auto observations =
        build_translation_unit_observations(result.compile_database.entries(), compile_commands_path);
    const auto include_resolution = include_resolver_port_.resolve_includes(observations);
    const auto observations_by_key = index_observations(observations);

    result.changed_file_key = resolve_changed_file_key(base_directory, changed_path);
    result.changed_file = display_changed_file(base_directory, result.changed_file_key);

    std::map<std::string, ImpactedTranslationUnit> impacted_by_key;
    const auto direct_match_found =
        collect_direct_impacts(observations, result.changed_file_key, impacted_by_key);
    const auto heuristic_match_found = collect_heuristic_impacts(
        include_resolution, result.changed_file_key, observations_by_key, impacted_by_key);

    for (const auto& [_, impacted] : impacted_by_key) {
        result.affected_translation_units.push_back(impacted);
    }

    sort_impacted_translation_units(result);
    append_resolution_diagnostics(result, include_resolution, impacted_by_key, direct_match_found,
                                  heuristic_match_found);
    append_impact_diagnostics(result, direct_match_found, heuristic_match_found);

    return result;
}

}  // namespace xray::hexagon::services
