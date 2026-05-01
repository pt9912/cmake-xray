#include "hexagon/services/target_graph_traversal.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hexagon/model/impact_result.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"

namespace xray::hexagon::services {

namespace {

using model::PrioritizedImpactedTarget;
using model::TargetDependency;
using model::TargetEvidenceStrength;
using model::TargetGraph;
using model::TargetInfo;
using model::TargetPriorityClass;

// Plan-M6-1-3.md "Sortierstaerke ueber explizite Rangtabelle":
// priority_class direct=0, direct_dependent=1, transitive_dependent=2.
// The if-chain pins the rank without relying on the enum's ordinal
// (which the plan forbids); the trailing return is the
// `transitive_dependent` arm, the only remaining case.
int rank_priority_class(TargetPriorityClass cls) {
    if (cls == TargetPriorityClass::direct) return 0;
    if (cls == TargetPriorityClass::direct_dependent) return 1;
    return 2;
}

// Plan-M6-1-3.md "Sortierstaerke ueber explizite Rangtabelle":
// evidence_strength direct=0, heuristic=1, uncertain=2.
int rank_evidence_strength(TargetEvidenceStrength strength) {
    if (strength == TargetEvidenceStrength::direct) return 0;
    if (strength == TargetEvidenceStrength::heuristic) return 1;
    return 2;
}

// Map graph_distance to the documented priority_class.
TargetPriorityClass priority_class_for_distance(std::size_t distance) {
    if (distance == 0) return TargetPriorityClass::direct;
    if (distance == 1) return TargetPriorityClass::direct_dependent;
    return TargetPriorityClass::transitive_dependent;
}

// "<external>::..."-keyed targets must never enter prioritized_targets;
// AP M6-1.3 explicitly drops them defense-in-depth even though AP 1.1's
// invariant already forbids externals from being `from` nodes.
constexpr std::string_view kExternalPrefix{"<external>::"};
bool is_external_unique_key(std::string_view key) {
    return key.starts_with(kExternalPrefix);
}

// Take the stronger of two evidence strengths per the rank table.
TargetEvidenceStrength stronger_evidence(TargetEvidenceStrength a,
                                          TargetEvidenceStrength b) {
    return rank_evidence_strength(a) <= rank_evidence_strength(b) ? a : b;
}

// Per-target reachability state during BFS. We keep `target` so the
// final sort and the prioritized_targets emission have all identity
// information without re-resolving from graph.nodes.
struct ReachedTarget {
    TargetInfo target;
    std::size_t distance;
    TargetEvidenceStrength evidence;
};

using ReachedMap = std::map<std::string, ReachedTarget>;
using IncomingIndex =
    std::map<std::string, std::vector<const TargetDependency*>>;
using NodeIndex = std::map<std::string, const TargetInfo*>;

void seed_reached_map(const std::vector<ImpactSeed>& seeds,
                      ReachedMap& reached) {
    for (const auto& seed : seeds) {
        if (is_external_unique_key(seed.target.unique_key)) continue;
        auto [it, inserted] = reached.emplace(
            seed.target.unique_key,
            ReachedTarget{seed.target, 0, seed.evidence_strength});
        if (!inserted) {
            it->second.evidence =
                stronger_evidence(it->second.evidence, seed.evidence_strength);
        }
    }
}

IncomingIndex build_incoming_index(const TargetGraph& graph,
                                    const NodeIndex& nodes_by_key) {
    IncomingIndex incoming_by_to;
    for (const auto& edge : graph.edges) {
        incoming_by_to[edge.to_unique_key].push_back(&edge);
    }
    // Determinism guard per plan-M6-1-3.md "ausgehende reverse Kanten
    // werden vor jedem Schritt nach
    // (stable_target_key, edge_kind, display_name, target_type)
    // sortiert". Comparator lives in internal:: for direct unit
    // testing of the tie-breaker chain.
    const auto node_for = [&nodes_by_key](const std::string& key)
        -> const TargetInfo* {
        const auto it = nodes_by_key.find(key);
        return it == nodes_by_key.end() ? nullptr : it->second;
    };
    for (auto& [_, edges] : incoming_by_to) {
        std::stable_sort(edges.begin(), edges.end(),
                         [&node_for](const TargetDependency* lhs,
                                     const TargetDependency* rhs) {
                             return internal::less_by_bfs_edge_tuple(
                                 *lhs, *rhs,
                                 node_for(lhs->from_unique_key),
                                 node_for(rhs->from_unique_key));
                         });
    }
    // NRVO-defeat per src/adapters/output/dot_report_adapter.cpp Z. 291.
    return IncomingIndex(std::move(incoming_by_to));
}

NodeIndex build_node_index(const TargetGraph& graph) {
    NodeIndex nodes_by_key;
    for (const auto& node : graph.nodes) {
        nodes_by_key.emplace(node.unique_key, &node);
    }
    return NodeIndex(std::move(nodes_by_key));
}

// Plan-M6-1-3.md "Frontier-Knoten ... werden vor jedem Schritt nach
// (stable_target_key, edge_kind, display_name, target_type) sortiert."
// edge_kind is only meaningful on edges; at node level the tuple
// collapses to (unique_key, display_name, type), implemented in
// internal::less_by_bfs_node_tuple so tests can drive every column.
std::vector<std::string> sorted_keys(const ReachedMap& reached) {
    std::vector<const ReachedTarget*> entries;
    entries.reserve(reached.size());
    for (const auto& [_, entry] : reached) entries.push_back(&entry);
    std::stable_sort(entries.begin(), entries.end(),
                      [](const ReachedTarget* lhs, const ReachedTarget* rhs) {
                          return internal::less_by_bfs_node_tuple(
                              lhs->target, rhs->target);
                      });
    std::vector<std::string> keys;
    keys.reserve(entries.size());
    for (const auto* entry : entries) keys.push_back(entry->target.unique_key);
    return std::vector<std::string>(std::move(keys));
}

void merge_candidate(ReachedMap& candidates, const TargetInfo& info,
                     std::size_t step, TargetEvidenceStrength evidence) {
    auto [it, inserted] = candidates.emplace(
        info.unique_key, ReachedTarget{info, step, evidence});
    if (!inserted) {
        it->second.evidence = stronger_evidence(it->second.evidence, evidence);
    }
}

ReachedMap expand_frontier_step(const std::vector<std::string>& frontier,
                                 const ReachedMap& reached,
                                 const IncomingIndex& incoming_by_to,
                                 const NodeIndex& nodes_by_key,
                                 std::size_t step) {
    ReachedMap candidates;
    for (const auto& current_key : frontier) {
        const auto edges_it = incoming_by_to.find(current_key);
        if (edges_it == incoming_by_to.end()) continue;
        const auto current_evidence = reached.at(current_key).evidence;
        for (const TargetDependency* edge : edges_it->second) {
            // Plan-M6-1-3.md "AP 1.3 verlaesst sich nicht allein auf
            // diese AP-1.1-Invariante: Kandidaten mit externem
            // Target-Key oder ohne interne TargetInfo werden ...
            // verworfen."
            if (is_external_unique_key(edge->from_unique_key)) continue;
            if (reached.find(edge->from_unique_key) != reached.end()) continue;
            const auto node_it = nodes_by_key.find(edge->from_unique_key);
            if (node_it == nodes_by_key.end()) continue;
            merge_candidate(candidates, *node_it->second, step, current_evidence);
        }
    }
    return ReachedMap(std::move(candidates));
}

std::vector<std::string> commit_candidates(ReachedMap& reached,
                                            ReachedMap candidates) {
    std::vector<std::string> next_frontier;
    next_frontier.reserve(candidates.size());
    for (auto& [key, candidate] : candidates) {
        reached.emplace(key, std::move(candidate));
        next_frontier.push_back(key);
    }
    return std::vector<std::string>(std::move(next_frontier));
}

std::vector<PrioritizedImpactedTarget> materialize_prioritized(
    ReachedMap reached) {
    std::vector<PrioritizedImpactedTarget> targets;
    targets.reserve(reached.size());
    for (auto& [_, entry] : reached) {
        PrioritizedImpactedTarget out_entry;
        out_entry.target = std::move(entry.target);
        out_entry.graph_distance = entry.distance;
        out_entry.priority_class = priority_class_for_distance(entry.distance);
        out_entry.evidence_strength = entry.evidence;
        targets.push_back(std::move(out_entry));
    }
    sort_prioritized_impacted_targets(targets);
    return std::vector<PrioritizedImpactedTarget>(std::move(targets));
}

}  // namespace

ReverseBfsResult reverse_target_graph_bfs(const TargetGraph& graph,
                                           const std::vector<ImpactSeed>& seeds,
                                           std::size_t requested_depth) {
    ReverseBfsResult result;
    if (seeds.empty()) return result;

    ReachedMap reached;
    seed_reached_map(seeds, reached);
    const auto nodes_by_key = build_node_index(graph);
    const auto incoming_by_to = build_incoming_index(graph, nodes_by_key);
    auto frontier = sorted_keys(reached);

    for (std::size_t step = 1; step <= requested_depth; ++step) {
        if (frontier.empty()) break;
        auto candidates =
            expand_frontier_step(frontier, reached, incoming_by_to,
                                 nodes_by_key, step);
        if (candidates.empty()) {
            result.stopped_early_no_more_targets = true;
            break;
        }
        result.effective_depth = step;
        frontier = commit_candidates(reached, std::move(candidates));
        if (step == requested_depth && !frontier.empty()) {
            result.stopped_at_depth_limit = true;
        }
    }

    result.prioritized_targets = materialize_prioritized(std::move(reached));
    return result;
}

void sort_prioritized_impacted_targets(
    std::vector<PrioritizedImpactedTarget>& targets) {
    // Sort tuple: (priority_class, graph_distance, evidence_strength,
    // unique_key). The trailing display_name / type tie-breakers from
    // the M6-Hauptplan-Sortier-Tupel are dropped here because BFS dedup
    // guarantees each unique_key appears at most once in the input.
    std::stable_sort(targets.begin(), targets.end(),
                      [](const PrioritizedImpactedTarget& lhs,
                         const PrioritizedImpactedTarget& rhs) {
                          const int lhs_p = rank_priority_class(lhs.priority_class);
                          const int rhs_p = rank_priority_class(rhs.priority_class);
                          if (lhs_p != rhs_p) return lhs_p < rhs_p;
                          if (lhs.graph_distance != rhs.graph_distance) {
                              return lhs.graph_distance < rhs.graph_distance;
                          }
                          const int lhs_e = rank_evidence_strength(lhs.evidence_strength);
                          const int rhs_e = rank_evidence_strength(rhs.evidence_strength);
                          if (lhs_e != rhs_e) return lhs_e < rhs_e;
                          return lhs.target.unique_key < rhs.target.unique_key;
                      });
}

namespace internal {

int rank_edge_kind(model::TargetDependencyKind kind) {
    if (kind == model::TargetDependencyKind::direct) return 0;
    return 1;  // reserved for future kinds; AP M6-1.3 only emits direct
}

bool less_by_bfs_node_tuple(const model::TargetInfo& lhs,
                            const model::TargetInfo& rhs) {
    if (lhs.unique_key != rhs.unique_key) {
        return lhs.unique_key < rhs.unique_key;
    }
    if (lhs.display_name != rhs.display_name) {
        return lhs.display_name < rhs.display_name;
    }
    return lhs.type < rhs.type;
}

bool less_by_bfs_edge_tuple(const model::TargetDependency& lhs,
                            const model::TargetDependency& rhs,
                            const model::TargetInfo* lhs_from_node,
                            const model::TargetInfo* rhs_from_node) {
    if (lhs.from_unique_key != rhs.from_unique_key) {
        return lhs.from_unique_key < rhs.from_unique_key;
    }
    const int lhs_kind = rank_edge_kind(lhs.kind);
    const int rhs_kind = rank_edge_kind(rhs.kind);
    if (lhs_kind != rhs_kind) return lhs_kind < rhs_kind;
    if (lhs.from_display_name != rhs.from_display_name) {
        return lhs.from_display_name < rhs.from_display_name;
    }
    const std::string_view lhs_type =
        lhs_from_node ? std::string_view{lhs_from_node->type} : std::string_view{};
    const std::string_view rhs_type =
        rhs_from_node ? std::string_view{rhs_from_node->type} : std::string_view{};
    return lhs_type < rhs_type;
}

}  // namespace internal

}  // namespace xray::hexagon::services
