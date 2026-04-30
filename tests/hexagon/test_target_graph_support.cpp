#include <doctest/doctest.h>

#include <cstddef>
#include <string>

#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"
#include "hexagon/services/target_graph_support.h"

namespace {

using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::services::compute_target_hubs;
using xray::hexagon::services::kDefaultTargetHubInThreshold;
using xray::hexagon::services::kDefaultTargetHubOutThreshold;
using xray::hexagon::services::sort_target_graph;
using xray::hexagon::services::TargetHubThresholds;

TargetInfo make_node(const std::string& unique_key,
                     const std::string& display_name = "",
                     const std::string& type = "STATIC_LIBRARY") {
    return TargetInfo{display_name.empty() ? unique_key : display_name, type,
                      unique_key};
}

TargetDependency make_edge(
    const std::string& from_uk, const std::string& to_uk,
    TargetDependencyResolution res = TargetDependencyResolution::resolved,
    const std::string& from_dn = "", const std::string& to_dn = "") {
    return TargetDependency{from_uk,
                            from_dn.empty() ? from_uk : from_dn,
                            to_uk,
                            to_dn.empty() ? to_uk : to_dn,
                            TargetDependencyKind::direct,
                            res};
}

TargetHubThresholds defaults() {
    return TargetHubThresholds{kDefaultTargetHubInThreshold,
                               kDefaultTargetHubOutThreshold};
}

}  // namespace

TEST_CASE("target_graph_support: empty graph is no-op for sort and yields empty hub lists") {
    TargetGraph g;
    sort_target_graph(g);
    CHECK(g.nodes.empty());
    CHECK(g.edges.empty());

    auto [in_hubs, out_hubs] = compute_target_hubs(g, defaults());
    CHECK(in_hubs.empty());
    CHECK(out_hubs.empty());
}

TEST_CASE("target_graph_support: single node without edges yields empty hub lists at default thresholds") {
    TargetGraph g;
    g.nodes.push_back(make_node("solo::STATIC_LIBRARY"));

    auto [in_hubs, out_hubs] = compute_target_hubs(g, defaults());
    CHECK(in_hubs.empty());
    CHECK(out_hubs.empty());
}

TEST_CASE("target_graph_support: ten incoming edges classify the target as in-hub at threshold 10 but not 11") {
    TargetGraph g;
    const std::string target_uk = "target::STATIC_LIBRARY";
    g.nodes.push_back(make_node(target_uk));
    for (int i = 0; i < 10; ++i) {
        const std::string src_uk = "src" + std::to_string(i) + "::STATIC_LIBRARY";
        g.nodes.push_back(make_node(src_uk));
        g.edges.push_back(make_edge(src_uk, target_uk));
    }
    sort_target_graph(g);

    SUBCASE("in_threshold=10 marks the target as in-hub") {
        auto [in_hubs, out_hubs] = compute_target_hubs(g, {10, 1000});
        REQUIRE(in_hubs.size() == 1);
        CHECK(in_hubs[0].unique_key == target_uk);
        CHECK(out_hubs.empty());
    }
    SUBCASE("in_threshold=11 leaves the target out of in-hubs") {
        auto [in_hubs, out_hubs] = compute_target_hubs(g, {11, 1000});
        CHECK(in_hubs.empty());
        CHECK(out_hubs.empty());
    }
}

TEST_CASE("target_graph_support: ten outgoing edges classify the source as out-hub at threshold 10") {
    TargetGraph g;
    const std::string src_uk = "src::STATIC_LIBRARY";
    g.nodes.push_back(make_node(src_uk));
    for (int i = 0; i < 10; ++i) {
        const std::string dst_uk = "dst" + std::to_string(i) + "::STATIC_LIBRARY";
        g.nodes.push_back(make_node(dst_uk));
        g.edges.push_back(make_edge(src_uk, dst_uk));
    }
    sort_target_graph(g);

    auto [in_hubs, out_hubs] = compute_target_hubs(g, {1000, 10});
    REQUIRE(out_hubs.size() == 1);
    CHECK(out_hubs[0].unique_key == src_uk);
    CHECK(in_hubs.empty());
}

TEST_CASE("target_graph_support: a node above both thresholds appears in both hub lists") {
    TargetGraph g;
    const std::string hub_uk = "hub::STATIC_LIBRARY";
    g.nodes.push_back(make_node(hub_uk));
    for (int i = 0; i < 10; ++i) {
        const std::string src_uk = "src" + std::to_string(i) + "::STATIC_LIBRARY";
        const std::string dst_uk = "dst" + std::to_string(i) + "::STATIC_LIBRARY";
        g.nodes.push_back(make_node(src_uk));
        g.nodes.push_back(make_node(dst_uk));
        g.edges.push_back(make_edge(src_uk, hub_uk));
        g.edges.push_back(make_edge(hub_uk, dst_uk));
    }
    sort_target_graph(g);

    auto [in_hubs, out_hubs] = compute_target_hubs(g, defaults());
    REQUIRE(in_hubs.size() == 1);
    REQUIRE(out_hubs.size() == 1);
    CHECK(in_hubs[0].unique_key == hub_uk);
    CHECK(out_hubs[0].unique_key == hub_uk);
}

TEST_CASE("target_graph_support: shuffled input sorts to the same canonical order as pre-sorted input") {
    TargetGraph shuffled;
    shuffled.nodes.push_back(make_node("c::STATIC_LIBRARY"));
    shuffled.nodes.push_back(make_node("a::STATIC_LIBRARY"));
    shuffled.nodes.push_back(make_node("b::STATIC_LIBRARY"));
    shuffled.edges.push_back(make_edge("c::STATIC_LIBRARY", "a::STATIC_LIBRARY"));
    shuffled.edges.push_back(make_edge("a::STATIC_LIBRARY", "b::STATIC_LIBRARY"));
    shuffled.edges.push_back(make_edge("b::STATIC_LIBRARY", "c::STATIC_LIBRARY"));

    TargetGraph pre_sorted;
    pre_sorted.nodes.push_back(make_node("a::STATIC_LIBRARY"));
    pre_sorted.nodes.push_back(make_node("b::STATIC_LIBRARY"));
    pre_sorted.nodes.push_back(make_node("c::STATIC_LIBRARY"));
    pre_sorted.edges.push_back(make_edge("a::STATIC_LIBRARY", "b::STATIC_LIBRARY"));
    pre_sorted.edges.push_back(make_edge("b::STATIC_LIBRARY", "c::STATIC_LIBRARY"));
    pre_sorted.edges.push_back(make_edge("c::STATIC_LIBRARY", "a::STATIC_LIBRARY"));

    sort_target_graph(shuffled);
    sort_target_graph(pre_sorted);

    REQUIRE(shuffled.nodes.size() == pre_sorted.nodes.size());
    for (std::size_t i = 0; i < shuffled.nodes.size(); ++i) {
        CHECK(shuffled.nodes[i].unique_key == pre_sorted.nodes[i].unique_key);
    }
    REQUIRE(shuffled.edges.size() == pre_sorted.edges.size());
    for (std::size_t i = 0; i < shuffled.edges.size(); ++i) {
        CHECK(shuffled.edges[i].from_unique_key ==
              pre_sorted.edges[i].from_unique_key);
        CHECK(shuffled.edges[i].to_unique_key ==
              pre_sorted.edges[i].to_unique_key);
    }
}

TEST_CASE("target_graph_support: external edges count toward source out-degree but the sentinel is not a hub") {
    TargetGraph g;
    const std::string src_uk = "src::STATIC_LIBRARY";
    g.nodes.push_back(make_node(src_uk));
    for (int i = 0; i < 10; ++i) {
        const std::string raw_id = "lib" + std::to_string(i);
        const std::string ext_uk = "<external>::" + raw_id;
        g.edges.push_back(make_edge(src_uk, ext_uk,
                                    TargetDependencyResolution::external, src_uk,
                                    raw_id));
    }
    sort_target_graph(g);

    auto [in_hubs, out_hubs] = compute_target_hubs(g, defaults());
    REQUIRE(out_hubs.size() == 1);
    CHECK(out_hubs[0].unique_key == src_uk);
    CHECK(in_hubs.empty());
}

TEST_CASE("target_graph_support: a deduped graph counts each entry exactly once") {
    TargetGraph g;
    const std::string target_uk = "target::STATIC_LIBRARY";
    g.nodes.push_back(make_node(target_uk));
    for (int i = 0; i < 10; ++i) {
        const std::string src_uk = "src" + std::to_string(i) + "::STATIC_LIBRARY";
        g.nodes.push_back(make_node(src_uk));
        g.edges.push_back(make_edge(src_uk, target_uk));
    }
    sort_target_graph(g);

    auto [in_hubs, out_hubs] = compute_target_hubs(g, defaults());
    REQUIRE(in_hubs.size() == 1);
    CHECK(in_hubs[0].unique_key == target_uk);
    CHECK(out_hubs.empty());
}

TEST_CASE("target_graph_support: node sort tie-breaks via display_name when unique_key collides") {
    TargetGraph g;
    g.nodes.push_back(TargetInfo{"zeta", "STATIC_LIBRARY", "shared::STATIC_LIBRARY"});
    g.nodes.push_back(TargetInfo{"alpha", "STATIC_LIBRARY", "shared::STATIC_LIBRARY"});
    sort_target_graph(g);
    REQUIRE(g.nodes.size() == 1);
    CHECK(g.nodes[0].display_name == "alpha");
}

TEST_CASE("target_graph_support: edge sort tie-breaks via display names when (from,to,kind) collide") {
    TargetGraph g;
    g.edges.push_back(TargetDependency{"a::STATIC_LIBRARY", "Z-from",
                                       "b::STATIC_LIBRARY", "Z-to",
                                       TargetDependencyKind::direct,
                                       TargetDependencyResolution::resolved});
    g.edges.push_back(TargetDependency{"a::STATIC_LIBRARY", "A-from",
                                       "b::STATIC_LIBRARY", "A-to",
                                       TargetDependencyKind::direct,
                                       TargetDependencyResolution::resolved});
    sort_target_graph(g);
    REQUIRE(g.edges.size() == 1);
    CHECK(g.edges[0].from_display_name == "A-from");
    CHECK(g.edges[0].to_display_name == "A-to");
}

TEST_CASE("target_graph_support: edge sort tie-breaks via to_display_name when (from,to,kind,from_display_name) all match") {
    // Pin the deepest tie-break tier: identical from_display_name forces the
    // comparator past the from-display tier and into the to-display tier.
    // Dedup still collapses to one edge afterwards.
    TargetGraph g;
    g.edges.push_back(TargetDependency{"a::STATIC_LIBRARY", "shared-from",
                                       "b::STATIC_LIBRARY", "Z-to",
                                       TargetDependencyKind::direct,
                                       TargetDependencyResolution::resolved});
    g.edges.push_back(TargetDependency{"a::STATIC_LIBRARY", "shared-from",
                                       "b::STATIC_LIBRARY", "A-to",
                                       TargetDependencyKind::direct,
                                       TargetDependencyResolution::resolved});
    sort_target_graph(g);
    REQUIRE(g.edges.size() == 1);
    CHECK(g.edges[0].to_display_name == "A-to");
}

TEST_CASE("target_graph_support: sort_target_graph is idempotent") {
    TargetGraph g;
    g.nodes.push_back(make_node("c::STATIC_LIBRARY"));
    g.nodes.push_back(make_node("a::STATIC_LIBRARY"));
    g.nodes.push_back(make_node("b::STATIC_LIBRARY"));
    g.edges.push_back(make_edge("a::STATIC_LIBRARY", "c::STATIC_LIBRARY"));
    g.edges.push_back(make_edge("b::STATIC_LIBRARY", "a::STATIC_LIBRARY"));
    sort_target_graph(g);

    const auto first_pass_nodes = g.nodes;
    const auto first_pass_edges = g.edges;
    sort_target_graph(g);

    REQUIRE(g.nodes.size() == first_pass_nodes.size());
    for (std::size_t i = 0; i < g.nodes.size(); ++i) {
        CHECK(g.nodes[i].unique_key == first_pass_nodes[i].unique_key);
        CHECK(g.nodes[i].display_name == first_pass_nodes[i].display_name);
        CHECK(g.nodes[i].type == first_pass_nodes[i].type);
    }
    REQUIRE(g.edges.size() == first_pass_edges.size());
    for (std::size_t i = 0; i < g.edges.size(); ++i) {
        CHECK(g.edges[i].from_unique_key == first_pass_edges[i].from_unique_key);
        CHECK(g.edges[i].to_unique_key == first_pass_edges[i].to_unique_key);
    }
}

TEST_CASE("target_graph_support: real target named 'external' does not collide with the <external>::* sentinel") {
    TargetGraph g;
    g.nodes.push_back(TargetInfo{"external", "STATIC_LIBRARY",
                                 "external::STATIC_LIBRARY"});
    const std::string src_uk = "src::STATIC_LIBRARY";
    g.nodes.push_back(make_node(src_uk));
    for (int i = 0; i < 10; ++i) {
        const std::string raw_id = "ext" + std::to_string(i);
        g.edges.push_back(make_edge(src_uk, "<external>::" + raw_id,
                                    TargetDependencyResolution::external, src_uk,
                                    raw_id));
    }
    sort_target_graph(g);

    REQUIRE(g.nodes.size() == 2);
    auto [in_hubs, out_hubs] = compute_target_hubs(g, defaults());
    REQUIRE(out_hubs.size() == 1);
    CHECK(out_hubs[0].unique_key == src_uk);
    CHECK(in_hubs.empty());
}

TEST_CASE("target_graph_support: dedup collapses three identical-unique_key nodes and three identical-identity edges") {
    TargetGraph g;
    g.nodes.push_back(TargetInfo{"X", "STATIC_LIBRARY", "X::STATIC_LIBRARY"});
    g.nodes.push_back(
        TargetInfo{"X-second", "STATIC_LIBRARY", "X::STATIC_LIBRARY"});
    g.nodes.push_back(
        TargetInfo{"X-third", "STATIC_LIBRARY", "X::STATIC_LIBRARY"});
    g.edges.push_back(TargetDependency{"A::STATIC_LIBRARY", "A1",
                                       "B::STATIC_LIBRARY", "B1",
                                       TargetDependencyKind::direct,
                                       TargetDependencyResolution::resolved});
    g.edges.push_back(TargetDependency{"A::STATIC_LIBRARY", "A2",
                                       "B::STATIC_LIBRARY", "B2",
                                       TargetDependencyKind::direct,
                                       TargetDependencyResolution::resolved});
    g.edges.push_back(TargetDependency{"A::STATIC_LIBRARY", "A3",
                                       "B::STATIC_LIBRARY", "B3",
                                       TargetDependencyKind::direct,
                                       TargetDependencyResolution::resolved});
    sort_target_graph(g);

    REQUIRE(g.nodes.size() == 1);
    CHECK(g.nodes[0].display_name == "X");
    REQUIRE(g.edges.size() == 1);
    CHECK(g.edges[0].from_display_name == "A1");
    CHECK(g.edges[0].to_display_name == "B1");
}

TEST_CASE("target_graph_support: external edges with different raw_id stay separate after sort") {
    TargetGraph g;
    g.edges.push_back(make_edge("from::STATIC_LIBRARY", "<external>::libfoo",
                                TargetDependencyResolution::external, "from",
                                "libfoo"));
    g.edges.push_back(make_edge("from::STATIC_LIBRARY", "<external>::libbar",
                                TargetDependencyResolution::external, "from",
                                "libbar"));
    sort_target_graph(g);

    REQUIRE(g.edges.size() == 2);
    CHECK(g.edges[0].to_unique_key == "<external>::libbar");
    CHECK(g.edges[1].to_unique_key == "<external>::libfoo");
}

TEST_CASE("target_graph_support: compute_target_hubs is robust against unsorted and duplicated input") {
    TargetGraph sorted_g;
    TargetGraph dirty_g;
    const std::string target_uk = "target::STATIC_LIBRARY";
    sorted_g.nodes.push_back(make_node(target_uk));
    dirty_g.nodes.push_back(make_node(target_uk));
    dirty_g.nodes.push_back(make_node(target_uk));
    for (int i = 0; i < 10; ++i) {
        const std::string src_uk = "src" + std::to_string(i) + "::STATIC_LIBRARY";
        sorted_g.nodes.push_back(make_node(src_uk));
        sorted_g.edges.push_back(make_edge(src_uk, target_uk));
        dirty_g.nodes.push_back(make_node(src_uk));
        dirty_g.edges.push_back(make_edge(src_uk, target_uk));
        dirty_g.edges.push_back(make_edge(src_uk, target_uk));
    }
    sort_target_graph(sorted_g);

    auto [sorted_in, sorted_out] = compute_target_hubs(sorted_g, defaults());
    auto [dirty_in, dirty_out] = compute_target_hubs(dirty_g, defaults());

    REQUIRE(sorted_in.size() == dirty_in.size());
    REQUIRE(sorted_out.size() == dirty_out.size());
    for (std::size_t i = 0; i < sorted_in.size(); ++i) {
        CHECK(sorted_in[i].unique_key == dirty_in[i].unique_key);
    }
}

TEST_CASE("target_graph_support: duplicate node entries collapse to one hub-list entry per unique_key") {
    TargetGraph g;
    const std::string target_uk = "target::STATIC_LIBRARY";
    g.nodes.push_back(make_node(target_uk));
    g.nodes.push_back(make_node(target_uk));
    for (int i = 0; i < 10; ++i) {
        const std::string src_uk = "src" + std::to_string(i) + "::STATIC_LIBRARY";
        g.nodes.push_back(make_node(src_uk));
        g.edges.push_back(make_edge(src_uk, target_uk));
    }

    auto [in_hubs, out_hubs] = compute_target_hubs(g, defaults());
    REQUIRE(in_hubs.size() == 1);
    CHECK(in_hubs[0].unique_key == target_uk);
}

TEST_CASE("target_graph_support: lex-smaller duplicate display_name updates the chosen hub-list entry") {
    // Pin the lex-smaller-wins branch in compute_target_hubs' duplicate-node
    // handler: insert the larger display_name first, then the smaller one,
    // and verify the smaller one wins the hub-list TargetInfo.
    TargetGraph g;
    const std::string shared_uk = "shared::STATIC_LIBRARY";
    g.nodes.push_back(TargetInfo{"zeta", "STATIC_LIBRARY", shared_uk});
    g.nodes.push_back(TargetInfo{"alpha", "STATIC_LIBRARY", shared_uk});
    for (int i = 0; i < 10; ++i) {
        const std::string src_uk = "src" + std::to_string(i) + "::STATIC_LIBRARY";
        g.nodes.push_back(make_node(src_uk));
        g.edges.push_back(make_edge(src_uk, shared_uk));
    }

    auto [in_hubs, out_hubs] = compute_target_hubs(g, defaults());
    REQUIRE(in_hubs.size() == 1);
    CHECK(in_hubs[0].unique_key == shared_uk);
    CHECK(in_hubs[0].display_name == "alpha");
}
