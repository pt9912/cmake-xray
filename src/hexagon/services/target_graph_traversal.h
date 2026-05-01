#pragma once

#include <cstddef>
#include <vector>

#include "hexagon/model/impact_result.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"

namespace xray::hexagon::services {

// AP M6-1.3 A.1: a single impact-seed target plus its evidence strength.
// Seeds are the graph_distance=0 starting points of the reverse-BFS over
// `target_graph.edges`. They come from owner-targets of impacted TUs
// (direct/heuristic) plus header-only or build-metadata-only targets
// (uncertain). See plan-M6-1-3.md "Algorithmus" Schritt 1.
struct ImpactSeed {
    model::TargetInfo target;
    model::TargetEvidenceStrength evidence_strength{
        model::TargetEvidenceStrength::direct};
};

// AP M6-1.3 A.1: deterministic reverse-BFS over `target_graph.edges`.
// The traversal walks edges *backwards*: starting from each seed it
// follows edges where the seed appears as `to_unique_key`, treating
// `from_unique_key` as the next-distance candidate.
//
// Determinism (plan-M6-1-3.md "Determinismus-Anforderungen"):
//   - frontier and edge sets are sorted by
//     (stable_target_key, edge_kind, display_name, target_type) before
//     each step;
//   - shortest distance wins; on ties the stronger evidence_strength
//     wins (direct > heuristic > uncertain);
//   - `<external>::*` candidates are dropped explicitly so external
//     targets never appear in `prioritized_targets`, even if a future
//     AP-1.1 change loosened the AP-1.1 invariant that externals are
//     always `to`-only;
//   - the visited-set guarantees cycle termination.
struct ReverseBfsResult {
    std::vector<model::PrioritizedImpactedTarget> prioritized_targets;
    // Largest reverse-BFS distance actually reached. 0 when the graph is
    // empty or no edges target a seed; capped by requested_depth.
    std::size_t effective_depth{0};
    // True iff the BFS exhausted the available frontier before reaching
    // requested_depth (no further reachable targets). Triggers the
    // "reverse target graph traversal stopped at depth N" note.
    bool stopped_early_no_more_targets{false};
    // True iff the BFS hit requested_depth while a non-empty frontier
    // was still being processed (i.e. the depth budget was the cap, not
    // the lack of further candidates). Triggers the "hit depth limit
    // within a cycle" warning.
    bool stopped_at_depth_limit{false};
};

ReverseBfsResult reverse_target_graph_bfs(const model::TargetGraph& graph,
                                          const std::vector<ImpactSeed>& seeds,
                                          std::size_t requested_depth);

// AP M6-1.3 A.1: deterministic 6-tuple sort for
// `prioritized_affected_targets`. Plan-M6-1-3.md "Sortierung" pins the
// tuple
//   (priority_class, graph_distance, evidence_strength,
//    stable_target_key, display_name, target_type)
// using explicit rank tables for the two enums (so a future enum
// reorder cannot silently re-permute reports).
void sort_prioritized_impacted_targets(
    std::vector<model::PrioritizedImpactedTarget>& targets);

}  // namespace xray::hexagon::services
