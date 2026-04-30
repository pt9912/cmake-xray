#include "services/target_graph_support.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace xray::hexagon::services {

namespace {

using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetInfo;

struct EdgeIdentity {
    std::string from_unique_key;
    std::string to_unique_key;
    TargetDependencyKind kind;

    bool operator==(const EdgeIdentity& other) const noexcept {
        return kind == other.kind && from_unique_key == other.from_unique_key
               && to_unique_key == other.to_unique_key;
    }
};

struct EdgeIdentityHash {
    std::size_t operator()(const EdgeIdentity& e) const noexcept {
        std::size_t h = std::hash<std::string>{}(e.from_unique_key);
        const std::size_t to_h = std::hash<std::string>{}(e.to_unique_key);
        h ^= to_h + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        const std::size_t kind_h =
            std::hash<int>{}(static_cast<int>(e.kind));
        h ^= kind_h + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

bool node_less(const TargetInfo& a, const TargetInfo& b) {
    return std::tie(a.unique_key, a.display_name, a.type) <
           std::tie(b.unique_key, b.display_name, b.type);
}

bool edge_less(const TargetDependency& a, const TargetDependency& b) {
    if (a.from_unique_key != b.from_unique_key)
        return a.from_unique_key < b.from_unique_key;
    if (a.to_unique_key != b.to_unique_key)
        return a.to_unique_key < b.to_unique_key;
    // TargetDependencyKind has only `direct` today; when adding a new kind,
    // insert an explicit rank-table tier here per plan-M6-1-1.md so order
    // does not depend on enum ordinal or string representation.
    if (a.from_display_name != b.from_display_name)
        return a.from_display_name < b.from_display_name;
    return a.to_display_name < b.to_display_name;
}

}  // namespace

void sort_target_graph(TargetGraph& graph) {
    std::sort(graph.nodes.begin(), graph.nodes.end(), node_less);
    {
        std::unordered_set<std::string> seen;
        seen.reserve(graph.nodes.size());
        const auto new_end = std::remove_if(
            graph.nodes.begin(), graph.nodes.end(),
            [&seen](const TargetInfo& n) {
                return !seen.insert(n.unique_key).second;
            });
        graph.nodes.erase(new_end, graph.nodes.end());
    }

    std::sort(graph.edges.begin(), graph.edges.end(), edge_less);
    {
        std::unordered_set<EdgeIdentity, EdgeIdentityHash> seen;
        seen.reserve(graph.edges.size());
        const auto new_end = std::remove_if(
            graph.edges.begin(), graph.edges.end(),
            [&seen](const TargetDependency& e) {
                return !seen.insert({e.from_unique_key, e.to_unique_key, e.kind})
                            .second;
            });
        graph.edges.erase(new_end, graph.edges.end());
    }
}

struct EdgeDegrees {
    std::unordered_map<std::string, std::size_t> in_count;
    std::unordered_map<std::string, std::size_t> out_count;
};

// Defensive identity-collapsed view of the node set. If a unique_key appears
// multiple times in graph.nodes (defect case), the lex-first (display_name,
// type) entry wins for the hub-list TargetInfo. Output parameter avoids the
// gcov NRVO artifact on the closing brace that named-return-of-unordered_map
// triggers on this toolchain.
void populate_deduped_node_info(
    const std::vector<TargetInfo>& nodes,
    std::unordered_map<std::string, TargetInfo>& node_info) {
    node_info.reserve(nodes.size());
    for (const auto& n : nodes) {
        const auto it = node_info.find(n.unique_key);
        if (it == node_info.end()) {
            node_info.emplace(n.unique_key, n);
            continue;
        }
        if (std::tie(n.display_name, n.type) <
            std::tie(it->second.display_name, it->second.type)) {
            it->second = n;
        }
    }
}

// Defensive edge-identity dedup so duplicate edges don't double-count toward
// in/out degree.
EdgeDegrees count_edge_degrees(const std::vector<TargetDependency>& edges) {
    std::unordered_set<EdgeIdentity, EdgeIdentityHash> seen;
    seen.reserve(edges.size());
    EdgeDegrees degrees;
    for (const auto& e : edges) {
        if (!seen.insert({e.from_unique_key, e.to_unique_key, e.kind}).second) {
            continue;
        }
        ++degrees.in_count[e.to_unique_key];
        ++degrees.out_count[e.from_unique_key];
    }
    return degrees;
}

std::size_t lookup_or_zero(const std::unordered_map<std::string, std::size_t>& m,
                           const std::string& key) {
    const auto it = m.find(key);
    return it == m.end() ? 0 : it->second;
}

std::pair<std::vector<TargetInfo>, std::vector<TargetInfo>>
compute_target_hubs(const TargetGraph& graph, TargetHubThresholds thresholds) {
    std::unordered_map<std::string, TargetInfo> node_info;
    populate_deduped_node_info(graph.nodes, node_info);
    const auto degrees = count_edge_degrees(graph.edges);

    std::vector<TargetInfo> in_hubs;
    std::vector<TargetInfo> out_hubs;
    in_hubs.reserve(node_info.size());
    out_hubs.reserve(node_info.size());
    for (const auto& [key, info] : node_info) {
        if (lookup_or_zero(degrees.in_count, key) >= thresholds.in_threshold)
            in_hubs.push_back(info);
        if (lookup_or_zero(degrees.out_count, key) >= thresholds.out_threshold)
            out_hubs.push_back(info);
    }

    std::sort(in_hubs.begin(), in_hubs.end(), node_less);
    std::sort(out_hubs.begin(), out_hubs.end(), node_less);
    return {std::move(in_hubs), std::move(out_hubs)};
}

}  // namespace xray::hexagon::services
