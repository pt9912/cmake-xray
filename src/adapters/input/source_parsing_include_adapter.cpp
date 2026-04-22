#include "adapters/input/source_parsing_include_adapter.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_set>
#include <utility>

#include "hexagon/model/diagnostic.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/model/translation_unit.h"
#include "hexagon/services/analysis_support.h"

namespace xray::adapters::input {

namespace {

using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::TranslationUnitObservation;

struct IncludeDirective {
    std::string value;
    bool is_quoted{false};
};

struct TraversalState {
    std::unordered_set<std::string> fully_visited;
    std::unordered_set<std::string> active_stack;
    std::unordered_set<std::string> resolved_headers;
    std::vector<Diagnostic> diagnostics;
};

std::vector<IncludeDirective> parse_include_directives(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) return {};

    const std::regex include_pattern{R"(^\s*#\s*include\s*([<"])([^">]+)[>"])"};

    std::vector<IncludeDirective> includes;
    std::string line;
    while (std::getline(file, line)) {
        std::smatch match;
        if (!std::regex_search(line, match, include_pattern)) continue;

        includes.push_back({.value = match[2].str(), .is_quoted = match[1].str() == "\""});
    }

    return includes;
}

std::filesystem::path try_resolve_include(
    const IncludeDirective& include_directive, const std::filesystem::path& current_file_path,
    const TranslationUnitObservation& translation_unit) {
    auto candidates = std::vector<std::filesystem::path>{};

    if (include_directive.is_quoted) {
        candidates.push_back(current_file_path.parent_path());
        for (const auto& path : translation_unit.quote_include_paths) {
            candidates.push_back(std::filesystem::path{path});
        }
    }

    for (const auto& path : translation_unit.include_paths) {
        candidates.push_back(std::filesystem::path{path});
    }
    for (const auto& path : translation_unit.system_include_paths) {
        candidates.push_back(std::filesystem::path{path});
    }

    for (const auto& candidate_directory : candidates) {
        const auto candidate_path =
            (candidate_directory / include_directive.value).lexically_normal();
        if (std::filesystem::exists(candidate_path)) return candidate_path;
    }

    return {};
}

std::string quote_style(const IncludeDirective& include_directive) {
    return include_directive.is_quoted ? "\"" : "<";
}

void note_unreadable_file(TraversalState& state,
                          const TranslationUnitObservation& translation_unit) {
    state.diagnostics.push_back(
        {DiagnosticSeverity::warning,
         "cannot read source file for include analysis: " +
             translation_unit.reference.source_path});
}

void note_unresolved_include(TraversalState& state, const IncludeDirective& include_directive,
                             const TranslationUnitObservation& translation_unit) {
    state.diagnostics.push_back(
        {DiagnosticSeverity::warning,
         "could not resolve include " + quote_style(include_directive) + include_directive.value +
             (include_directive.is_quoted ? "\"" : ">") + " from " +
             translation_unit.reference.source_path});
}

void traverse_includes(const std::filesystem::path& file_path,
                       const TranslationUnitObservation& translation_unit,
                       TraversalState& state) {
    const auto normalized_file = xray::hexagon::services::normalize_path(file_path);
    if (!state.active_stack.insert(normalized_file).second) return;
    if (!state.fully_visited.insert(normalized_file).second) {
        state.active_stack.erase(normalized_file);
        return;
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        note_unreadable_file(state, translation_unit);
        state.active_stack.erase(normalized_file);
        return;
    }

    file.close();

    for (const auto& include_directive : parse_include_directives(file_path)) {
        const auto resolved_path =
            try_resolve_include(include_directive, file_path, translation_unit);
        if (resolved_path.empty()) {
            note_unresolved_include(state, include_directive, translation_unit);
            continue;
        }

        const auto header_key = xray::hexagon::services::normalize_path(resolved_path);
        state.resolved_headers.insert(header_key);

        if (state.active_stack.find(header_key) != state.active_stack.end()) continue;

        traverse_includes(resolved_path, translation_unit, state);
    }

    state.active_stack.erase(normalized_file);
}

}  // namespace

IncludeResolutionResult SourceParsingIncludeAdapter::resolve_includes(
    const std::vector<TranslationUnitObservation>& translation_units) const {
    IncludeResolutionResult result;
    result.heuristic = true;

    result.translation_units.reserve(translation_units.size());
    for (const auto& translation_unit : translation_units) {
        TraversalState state;

        traverse_includes(std::filesystem::path{translation_unit.resolved_source_path},
                          translation_unit, state);

        std::vector<std::string> headers{state.resolved_headers.begin(), state.resolved_headers.end()};
        std::sort(headers.begin(), headers.end());

        result.translation_units.push_back({
            .translation_unit_key = translation_unit.reference.unique_key,
            .headers = std::move(headers),
            .diagnostics = std::move(state.diagnostics),
        });
    }

    return result;
}

}  // namespace xray::adapters::input
