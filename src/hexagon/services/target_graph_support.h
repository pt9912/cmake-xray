#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"

namespace xray::hexagon::services {

struct TargetHubThresholds {
    std::size_t in_threshold;
    std::size_t out_threshold;
};

inline constexpr std::size_t kDefaultTargetHubInThreshold = 10;
inline constexpr std::size_t kDefaultTargetHubOutThreshold = 10;

// Single-source-of-truth for TargetGraph identity normalization.
// Sorts nodes by (unique_key, display_name, type) and edges by
// (from_unique_key, to_unique_key, kind, from_display_name, to_display_name),
// then dedups nodes by unique_key and edges by (from_unique_key,
// to_unique_key, kind). First entry in sort order wins for display fields.
// Idempotent.
void sort_target_graph(model::TargetGraph& graph);

// Counts in/out edges per unique_key and returns (in_hubs, out_hubs).
// Defensive against unsorted or duplicated input via internal identity sets.
// External edges count toward the source's out-degree and the
// "<external>::*"-keyed target's in-degree, but only nodes from
// graph.nodes can appear in the returned hub lists. Hub lists are sorted
// by (unique_key, display_name, type).
std::pair<std::vector<model::TargetInfo>, std::vector<model::TargetInfo>>
compute_target_hubs(const model::TargetGraph& graph,
                    TargetHubThresholds thresholds);

}  // namespace xray::hexagon::services
