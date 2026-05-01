#include <doctest/doctest.h>

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "hexagon/model/impact_result.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"
#include "hexagon/services/target_graph_traversal.h"

namespace {

using xray::hexagon::model::PrioritizedImpactedTarget;
using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetEvidenceStrength;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TargetPriorityClass;
using xray::hexagon::services::ImpactSeed;
using xray::hexagon::services::reverse_target_graph_bfs;
using xray::hexagon::services::sort_prioritized_impacted_targets;

TargetInfo make_target(std::string name, std::string type = "STATIC_LIBRARY") {
    return TargetInfo{name, type, name + "::" + type};
}

TargetDependency edge(std::string from_name, std::string to_name,
                      std::string from_type = "STATIC_LIBRARY",
                      std::string to_type = "STATIC_LIBRARY") {
    return TargetDependency{from_name + "::" + from_type, from_name,
                            to_name + "::" + to_type,     to_name,
                            TargetDependencyKind::direct,
                            TargetDependencyResolution::resolved};
}

}  // namespace

TEST_CASE("reverse-bfs: empty seeds yield empty result and effective_depth=0") {
    TargetGraph graph;
    graph.nodes.push_back(make_target("a"));
    const auto result = reverse_target_graph_bfs(graph, {}, 5);
    CHECK(result.prioritized_targets.empty());
    CHECK(result.effective_depth == 0);
    CHECK_FALSE(result.stopped_early_no_more_targets);
    CHECK_FALSE(result.stopped_at_depth_limit);
}

TEST_CASE("reverse-bfs: single seed without incoming edges emits the seed at distance 0") {
    TargetGraph graph;
    graph.nodes.push_back(make_target("hub"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("hub"), TargetEvidenceStrength::direct},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 2);
    REQUIRE(result.prioritized_targets.size() == 1);
    const auto& entry = result.prioritized_targets[0];
    CHECK(entry.target.unique_key == "hub::STATIC_LIBRARY");
    CHECK(entry.graph_distance == 0);
    CHECK(entry.priority_class == TargetPriorityClass::direct);
    CHECK(entry.evidence_strength == TargetEvidenceStrength::direct);
    CHECK(result.effective_depth == 0);
    // No incoming edges: BFS attempts step 1 with empty next-frontier and
    // reports stopped_early_no_more_targets so callers can emit the
    // "stopped at depth N" note when requested_depth > effective_depth.
    CHECK(result.stopped_early_no_more_targets);
}

TEST_CASE("reverse-bfs: hop from direct seed inherits the direct evidence and distance=1") {
    TargetGraph graph;
    graph.nodes.push_back(make_target("hub"));
    graph.nodes.push_back(make_target("dependent"));
    graph.edges.push_back(edge("dependent", "hub"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("hub"), TargetEvidenceStrength::direct},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 2);
    REQUIRE(result.prioritized_targets.size() == 2);
    // Sorted by (priority_class, distance, ...) → direct (distance 0)
    // before direct_dependent (distance 1).
    CHECK(result.prioritized_targets[0].target.unique_key == "hub::STATIC_LIBRARY");
    CHECK(result.prioritized_targets[0].graph_distance == 0);
    CHECK(result.prioritized_targets[1].target.unique_key == "dependent::STATIC_LIBRARY");
    CHECK(result.prioritized_targets[1].graph_distance == 1);
    CHECK(result.prioritized_targets[1].priority_class ==
          TargetPriorityClass::direct_dependent);
    CHECK(result.prioritized_targets[1].evidence_strength ==
          TargetEvidenceStrength::direct);
    CHECK(result.effective_depth == 1);
}

TEST_CASE("reverse-bfs: hop from heuristic seed propagates heuristic evidence") {
    TargetGraph graph;
    graph.nodes.push_back(make_target("h"));
    graph.nodes.push_back(make_target("d"));
    graph.edges.push_back(edge("d", "h"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("h"), TargetEvidenceStrength::heuristic},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 2);
    REQUIRE(result.prioritized_targets.size() == 2);
    CHECK(result.prioritized_targets[1].evidence_strength ==
          TargetEvidenceStrength::heuristic);
}

TEST_CASE("reverse-bfs: uncertain seed maps to graph_distance=0 priority_class=direct") {
    // "uncertain" seeds (header-only-library / build-metadata-only) keep
    // priority_class=direct because they ARE the change-impact entry
    // point; evidence_strength carries the hint that there is no TU
    // anchor for the classification.
    TargetGraph graph;
    graph.nodes.push_back(make_target("hh"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("hh"), TargetEvidenceStrength::uncertain},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 0);
    REQUIRE(result.prioritized_targets.size() == 1);
    CHECK(result.prioritized_targets[0].priority_class == TargetPriorityClass::direct);
    CHECK(result.prioritized_targets[0].graph_distance == 0);
    CHECK(result.prioritized_targets[0].evidence_strength ==
          TargetEvidenceStrength::uncertain);
}

TEST_CASE("reverse-bfs: depth budget caps the traversal; stopped_at_depth_limit fires") {
    // chain: seed=hub  <-  dep1  <-  dep2  <-  dep3
    TargetGraph graph;
    for (const auto& name : {"hub", "dep1", "dep2", "dep3"}) {
        graph.nodes.push_back(make_target(name));
    }
    graph.edges.push_back(edge("dep1", "hub"));
    graph.edges.push_back(edge("dep2", "dep1"));
    graph.edges.push_back(edge("dep3", "dep2"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("hub"), TargetEvidenceStrength::direct},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 2);
    // depth=2 → seed + dep1 + dep2; dep3 is cut off.
    REQUIRE(result.prioritized_targets.size() == 3);
    CHECK(result.effective_depth == 2);
    CHECK(result.stopped_at_depth_limit);
    CHECK_FALSE(result.stopped_early_no_more_targets);
}

TEST_CASE("reverse-bfs: depth=0 emits only the seeds, no BFS hop") {
    TargetGraph graph;
    graph.nodes.push_back(make_target("hub"));
    graph.nodes.push_back(make_target("d"));
    graph.edges.push_back(edge("d", "hub"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("hub"), TargetEvidenceStrength::direct},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 0);
    REQUIRE(result.prioritized_targets.size() == 1);
    CHECK(result.prioritized_targets[0].target.unique_key == "hub::STATIC_LIBRARY");
    CHECK(result.effective_depth == 0);
    CHECK_FALSE(result.stopped_at_depth_limit);
}

TEST_CASE("reverse-bfs: cycle terminates via visited-set; shortest distance wins") {
    // a -> b -> c -> a (cycle); seed = a.
    TargetGraph graph;
    graph.nodes.push_back(make_target("a"));
    graph.nodes.push_back(make_target("b"));
    graph.nodes.push_back(make_target("c"));
    graph.edges.push_back(edge("b", "a"));
    graph.edges.push_back(edge("c", "b"));
    graph.edges.push_back(edge("a", "c"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("a"), TargetEvidenceStrength::direct},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 5);
    // a (0), b (1), c (2). a's reverse-BFS would re-enter a at distance
    // 3 via the cycle but the visited-set drops the longer path; the
    // result must contain each target exactly once at its shortest
    // distance.
    REQUIRE(result.prioritized_targets.size() == 3);
    std::map<std::string, std::size_t> distance_by_key;
    for (const auto& entry : result.prioritized_targets) {
        distance_by_key[entry.target.unique_key] = entry.graph_distance;
    }
    CHECK(distance_by_key["a::STATIC_LIBRARY"] == 0);
    CHECK(distance_by_key["b::STATIC_LIBRARY"] == 1);
    CHECK(distance_by_key["c::STATIC_LIBRARY"] == 2);
    CHECK(result.effective_depth == 2);
}

TEST_CASE("reverse-bfs: same target reachable via direct and heuristic seeds picks direct evidence at same distance") {
    // Two seeds at distance 0 with different evidence; same target.
    TargetGraph graph;
    graph.nodes.push_back(make_target("x"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("x"), TargetEvidenceStrength::heuristic},
        {make_target("x"), TargetEvidenceStrength::direct},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 0);
    REQUIRE(result.prioritized_targets.size() == 1);
    CHECK(result.prioritized_targets[0].evidence_strength ==
          TargetEvidenceStrength::direct);
}

TEST_CASE("reverse-bfs: same target reachable via direct and heuristic hops at same distance picks direct evidence") {
    // Two seeds (one direct, one heuristic), both reach `shared`
    // at distance 1.
    TargetGraph graph;
    for (const auto& name : {"hub_d", "hub_h", "shared"}) {
        graph.nodes.push_back(make_target(name));
    }
    graph.edges.push_back(edge("shared", "hub_d"));
    graph.edges.push_back(edge("shared", "hub_h"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("hub_d"), TargetEvidenceStrength::direct},
        {make_target("hub_h"), TargetEvidenceStrength::heuristic},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 2);
    // Three entries: hub_d (0/direct), hub_h (0/heuristic), shared (1/?)
    REQUIRE(result.prioritized_targets.size() == 3);
    PrioritizedImpactedTarget shared_entry;
    for (const auto& entry : result.prioritized_targets) {
        if (entry.target.unique_key == "shared::STATIC_LIBRARY") {
            shared_entry = entry;
        }
    }
    REQUIRE(shared_entry.target.unique_key == "shared::STATIC_LIBRARY");
    CHECK(shared_entry.graph_distance == 1);
    CHECK(shared_entry.evidence_strength == TargetEvidenceStrength::direct);
}

TEST_CASE("reverse-bfs: external <external>::* candidates are dropped defensively") {
    // Even if a future model change made externals appear as a `from`
    // node, AP 1.3 must not emit them in prioritized_targets.
    TargetGraph graph;
    graph.nodes.push_back(make_target("internal"));
    graph.edges.push_back(TargetDependency{
        "<external>::ghost", "ghost", "internal::STATIC_LIBRARY", "internal",
        TargetDependencyKind::direct, TargetDependencyResolution::external});
    const std::vector<ImpactSeed> seeds = {
        {make_target("internal"), TargetEvidenceStrength::direct},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 2);
    REQUIRE(result.prioritized_targets.size() == 1);
    CHECK(result.prioritized_targets[0].target.unique_key ==
          "internal::STATIC_LIBRARY");
}

TEST_CASE("reverse-bfs: stopped_early_no_more_targets fires when frontier exhausts before requested_depth") {
    TargetGraph graph;
    graph.nodes.push_back(make_target("hub"));
    graph.nodes.push_back(make_target("d"));
    graph.edges.push_back(edge("d", "hub"));
    const std::vector<ImpactSeed> seeds = {
        {make_target("hub"), TargetEvidenceStrength::direct},
    };
    const auto result = reverse_target_graph_bfs(graph, seeds, 5);
    // BFS reaches d at step 1; step 2's frontier is empty.
    CHECK(result.effective_depth == 1);
    CHECK(result.stopped_early_no_more_targets);
    CHECK_FALSE(result.stopped_at_depth_limit);
}

TEST_CASE("sort_prioritized_impacted_targets follows the documented 4-tuple") {
    std::vector<PrioritizedImpactedTarget> targets;
    targets.push_back({make_target("z"), TargetPriorityClass::direct, 0,
                       TargetEvidenceStrength::direct});
    // Two transitive_dependent entries at distinct distances exercise
    // the graph_distance tie-breaker after equal priority_class.
    targets.push_back({make_target("a"), TargetPriorityClass::transitive_dependent, 3,
                       TargetEvidenceStrength::direct});
    targets.push_back({make_target("c"), TargetPriorityClass::transitive_dependent, 2,
                       TargetEvidenceStrength::uncertain});
    targets.push_back({make_target("b"), TargetPriorityClass::direct_dependent, 1,
                       TargetEvidenceStrength::heuristic});
    targets.push_back({make_target("a", "EXECUTABLE"),
                       TargetPriorityClass::direct_dependent, 1,
                       TargetEvidenceStrength::direct});
    // direct_dependent + distance 1 + uncertain evidence: triggers the
    // evidence_strength tie-breaker against the other direct_dependent
    // entries so the uncertain rank is exercised.
    targets.push_back({make_target("d"), TargetPriorityClass::direct_dependent, 1,
                       TargetEvidenceStrength::uncertain});
    sort_prioritized_impacted_targets(targets);
    // Expected order:
    //   priority_class direct: z (0/direct)
    //   direct_dependent (distance 1): a::EXECUTABLE (direct) before
    //     b (heuristic) before d (uncertain) by evidence_strength rank
    //   transitive_dependent: c (2/uncertain) before a (3/direct);
    //     graph_distance dominates the evidence tie-breaker.
    REQUIRE(targets.size() == 6);
    CHECK(targets[0].target.unique_key == "z::STATIC_LIBRARY");
    CHECK(targets[1].target.unique_key == "a::EXECUTABLE");
    CHECK(targets[2].target.unique_key == "b::STATIC_LIBRARY");
    CHECK(targets[3].target.unique_key == "d::STATIC_LIBRARY");
    CHECK(targets[4].target.unique_key == "c::STATIC_LIBRARY");
    CHECK(targets[5].target.unique_key == "a::STATIC_LIBRARY");
}
