#include "adapters/input/source_parsing_include_adapter.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "hexagon/model/diagnostic.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/include_resolution.h"
#include "hexagon/model/translation_unit.h"
#include "hexagon/services/analysis_support.h"

namespace xray::adapters::input {

namespace {

using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::IncludeDepthKind;
using xray::hexagon::model::IncludeEntry;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::ResolvedTranslationUnitIncludes;
using xray::hexagon::model::TranslationUnitObservation;

constexpr std::size_t kIncludeDepthLimit = 32;
constexpr std::size_t kIncludeNodeBudget = 10000;

struct IncludeDirective {
    std::string value;
    bool is_quoted{false};
};

std::string raw_spelling_of(const IncludeDirective& directive) {
    return directive.is_quoted ? "\"" + directive.value + "\"" : "<" + directive.value + ">";
}

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

void note_unreadable_file(std::vector<Diagnostic>& diagnostics,
                          const TranslationUnitObservation& translation_unit) {
    diagnostics.push_back(
        {DiagnosticSeverity::warning,
         "cannot read source file for include analysis: " +
             translation_unit.reference.source_path});
}

void note_unresolved_include(std::vector<Diagnostic>& diagnostics,
                             const IncludeDirective& include_directive,
                             const TranslationUnitObservation& translation_unit) {
    diagnostics.push_back(
        {DiagnosticSeverity::warning,
         "could not resolve include " + quote_style(include_directive) + include_directive.value +
             (include_directive.is_quoted ? "\"" : ">") + " from " +
             translation_unit.reference.source_path});
}

struct GlobalBudgetState {
    std::size_t include_depth_limit_effective{0};
    std::size_t include_node_budget_effective{0};
    bool include_node_budget_reached{false};
    bool include_depth_limit_was_hit{false};
};

struct FrontierNode {
    std::filesystem::path path;
    std::string normalized_path;
    std::size_t depth{0};
};

struct PendingEdge {
    std::string including_normalized;
    std::string included_normalized;
    std::filesystem::path included_path;
    std::string raw_spelling;
    std::size_t target_depth{0};
};

bool pending_edge_less(const PendingEdge& lhs, const PendingEdge& rhs) {
    return std::tie(lhs.including_normalized, lhs.included_normalized, lhs.raw_spelling) <
           std::tie(rhs.including_normalized, rhs.included_normalized, rhs.raw_spelling);
}

struct TranslationUnitState {
    ResolvedTranslationUnitIncludes result;
    std::unordered_set<std::string> traversal_visited;
    std::set<std::pair<std::string, IncludeDepthKind>> emitted_depth_kinds;
    bool start_file_attempted{false};
};

bool can_read_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    return file.is_open();
}

void gather_edges_from_node(const FrontierNode& node,
                            const TranslationUnitObservation& translation_unit,
                            TranslationUnitState& tu_state, GlobalBudgetState& global,
                            std::vector<PendingEdge>& edges) {
    if (node.depth >= kIncludeDepthLimit) {
        global.include_depth_limit_was_hit = true;
        return;
    }
    if (!can_read_file(node.path)) {
        if (node.depth == 0 && !tu_state.start_file_attempted) {
            note_unreadable_file(tu_state.result.diagnostics, translation_unit);
            tu_state.start_file_attempted = true;
        }
        return;
    }
    if (node.depth == 0) tu_state.start_file_attempted = true;

    for (const auto& directive : parse_include_directives(node.path)) {
        const auto resolved_path = try_resolve_include(directive, node.path, translation_unit);
        if (resolved_path.empty()) {
            note_unresolved_include(tu_state.result.diagnostics, directive, translation_unit);
            continue;
        }
        edges.push_back(PendingEdge{
            node.normalized_path,
            xray::hexagon::services::normalize_path(resolved_path),
            resolved_path,
            raw_spelling_of(directive),
            node.depth + 1,
        });
    }
}

void emit_include_entry(TranslationUnitState& tu_state, GlobalBudgetState& global,
                        const std::string& header_path, IncludeDepthKind depth_kind,
                        std::size_t target_depth) {
    tu_state.result.headers.push_back(IncludeEntry{header_path, depth_kind});
    tu_state.emitted_depth_kinds.insert(std::make_pair(header_path, depth_kind));
    if (target_depth > global.include_depth_limit_effective) {
        global.include_depth_limit_effective = target_depth;
    }
}

void process_pending_edge(const PendingEdge& edge, TranslationUnitState& tu_state,
                          GlobalBudgetState& global,
                          std::vector<FrontierNode>& next_frontier) {
    const auto depth_kind =
        (edge.target_depth == 1) ? IncludeDepthKind::direct : IncludeDepthKind::indirect;
    const auto already_visited =
        tu_state.traversal_visited.find(edge.included_normalized) !=
        tu_state.traversal_visited.end();

    if (already_visited) {
        const auto key = std::make_pair(edge.included_normalized, depth_kind);
        if (tu_state.emitted_depth_kinds.find(key) == tu_state.emitted_depth_kinds.end()) {
            emit_include_entry(tu_state, global, edge.included_normalized, depth_kind,
                               edge.target_depth);
        }
        return;
    }

    if (global.include_node_budget_effective >= kIncludeNodeBudget) {
        global.include_node_budget_reached = true;
        return;
    }

    ++global.include_node_budget_effective;
    tu_state.traversal_visited.insert(edge.included_normalized);
    emit_include_entry(tu_state, global, edge.included_normalized, depth_kind, edge.target_depth);
    next_frontier.push_back({edge.included_path, edge.included_normalized, edge.target_depth});
}

ResolvedTranslationUnitIncludes resolve_for_translation_unit(
    const TranslationUnitObservation& translation_unit, GlobalBudgetState& global) {
    TranslationUnitState tu_state;
    tu_state.result.translation_unit_key = translation_unit.reference.unique_key;

    const auto start_path = std::filesystem::path{translation_unit.resolved_source_path};
    const auto start_normalized = xray::hexagon::services::normalize_path(start_path);
    tu_state.traversal_visited.insert(start_normalized);

    std::vector<FrontierNode> current_frontier;
    current_frontier.push_back({start_path, start_normalized, 0});

    while (!current_frontier.empty()) {
        std::vector<PendingEdge> edges;
        for (const auto& node : current_frontier) {
            gather_edges_from_node(node, translation_unit, tu_state, global, edges);
        }

        std::sort(edges.begin(), edges.end(), pending_edge_less);

        std::vector<FrontierNode> next_frontier;
        for (const auto& edge : edges) {
            process_pending_edge(edge, tu_state, global, next_frontier);
        }

        current_frontier = std::move(next_frontier);
    }

    std::sort(tu_state.result.headers.begin(), tu_state.result.headers.end(),
              [](const IncludeEntry& lhs, const IncludeEntry& rhs) {
                  if (lhs.header_path != rhs.header_path) return lhs.header_path < rhs.header_path;
                  return static_cast<int>(lhs.depth_kind) < static_cast<int>(rhs.depth_kind);
              });

    return std::move(tu_state.result);
}

bool translation_unit_observation_less(const TranslationUnitObservation& lhs,
                                       const TranslationUnitObservation& rhs) {
    if (lhs.reference.source_path_key != rhs.reference.source_path_key) {
        return lhs.reference.source_path_key < rhs.reference.source_path_key;
    }
    return lhs.reference.unique_key < rhs.reference.unique_key;
}

}  // namespace

IncludeResolutionResult SourceParsingIncludeAdapter::resolve_includes(
    const std::vector<TranslationUnitObservation>& translation_units) const {
    IncludeResolutionResult result;
    result.heuristic = true;
    result.include_depth_limit_requested = kIncludeDepthLimit;
    result.include_node_budget_requested = kIncludeNodeBudget;

    std::vector<TranslationUnitObservation> sorted_observations{translation_units.begin(),
                                                                translation_units.end()};
    std::sort(sorted_observations.begin(), sorted_observations.end(),
              translation_unit_observation_less);

    GlobalBudgetState global;

    result.translation_units.reserve(sorted_observations.size());
    for (const auto& observation : sorted_observations) {
        result.translation_units.push_back(resolve_for_translation_unit(observation, global));
    }
    result.include_depth_limit_effective = global.include_depth_limit_effective;
    result.include_node_budget_effective = global.include_node_budget_effective;
    result.include_node_budget_reached = global.include_node_budget_reached;

    if (global.include_node_budget_reached) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::note,
             "include analysis stopped at " +
                 std::to_string(global.include_node_budget_effective) +
                 " nodes (budget reached); some indirect includes may be missing"});
    }
    if (global.include_depth_limit_was_hit) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::note,
             "include analysis stopped at depth " + std::to_string(kIncludeDepthLimit) +
                 " (depth limit reached); deeper transitive includes may be missing"});
    }

    return result;
}

}  // namespace xray::adapters::input
