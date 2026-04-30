#include "services/target_graph_support.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
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

// Explicit rank table: edge sort order must not depend on enum ordinal or
// string representation of the kind, so future enum additions stay
// reorder-safe.
int kind_rank(TargetDependencyKind kind) {
    switch (kind) {
        case TargetDependencyKind::direct:
            return 0;
    }
    return std::numeric_limits<int>::max();
}

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
    const int ar = kind_rank(a.kind);
    const int br = kind_rank(b.kind);
    if (ar != br) return ar < br;
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

std::pair<std::vector<TargetInfo>, std::vector<TargetInfo>>
compute_target_hubs(const TargetGraph& graph, TargetHubThresholds thresholds) {
    // Defensive identity-collapsed view of the node set. If a unique_key
    // appears multiple times in graph.nodes (defect case), the lex-first
    // (display_name, type) entry wins for the hub-list TargetInfo.
    std::unordered_map<std::string, TargetInfo> node_info;
    node_info.reserve(graph.nodes.size());
    for (const auto& n : graph.nodes) {
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

    // Defensive edge-identity dedup so duplicate edges don't double-count.
    std::unordered_set<EdgeIdentity, EdgeIdentityHash> seen_edges;
    seen_edges.reserve(graph.edges.size());
    std::unordered_map<std::string, std::size_t> in_count;
    std::unordered_map<std::string, std::size_t> out_count;
    for (const auto& e : graph.edges) {
        if (!seen_edges.insert({e.from_unique_key, e.to_unique_key, e.kind})
                 .second) {
            continue;
        }
        ++in_count[e.to_unique_key];
        ++out_count[e.from_unique_key];
    }

    std::vector<TargetInfo> in_hubs;
    std::vector<TargetInfo> out_hubs;
    in_hubs.reserve(node_info.size());
    out_hubs.reserve(node_info.size());
    for (const auto& [key, info] : node_info) {
        const auto in_it = in_count.find(key);
        const auto out_it = out_count.find(key);
        const std::size_t in_c = (in_it == in_count.end()) ? 0 : in_it->second;
        const std::size_t out_c =
            (out_it == out_count.end()) ? 0 : out_it->second;
        if (in_c >= thresholds.in_threshold) in_hubs.push_back(info);
        if (out_c >= thresholds.out_threshold) out_hubs.push_back(info);
    }

    auto sort_hubs = [](std::vector<TargetInfo>& v) {
        std::sort(v.begin(), v.end(), node_less);
    };
    sort_hubs(in_hubs);
    sort_hubs(out_hubs);
    return {std::move(in_hubs), std::move(out_hubs)};
}

}  // namespace xray::hexagon::services
