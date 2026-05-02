#include "adapters/output/dot_report_adapter.h"

#include "adapters/output/impact_priority_text.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/include_hotspot.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/target_graph.h"
#include "hexagon/model/target_info.h"
#include "hexagon/model/translation_unit.h"

namespace xray::adapters::output {

DotBudget compute_analyze_budget(std::size_t top_limit) {
    // spec/report-dot.md: context_limit = min(top_limit, 5);
    //                    node_limit = max(25, 4 * top_limit + 10);
    //                    edge_limit = max(40, 6 * top_limit + 20).
    constexpr std::size_t kTopLimitClamp = 100'000;
    const auto bounded_top = top_limit < kTopLimitClamp ? top_limit : kTopLimitClamp;
    DotBudget budget{};
    budget.context_limit = bounded_top < 5 ? bounded_top : 5;
    const auto node_candidate = 4 * bounded_top + 10;
    budget.node_limit = node_candidate < 25 ? std::size_t{25} : node_candidate;
    const auto edge_candidate = 6 * bounded_top + 20;
    budget.edge_limit = edge_candidate < 40 ? std::size_t{40} : edge_candidate;
    return budget;
}

DotBudget compute_impact_budget() {
    // spec/report-dot.md: fixed M5 budgets, node_limit = 100, edge_limit = 200.
    DotBudget budget{};
    budget.context_limit = 0;
    budget.node_limit = 100;
    budget.edge_limit = 200;
    return budget;
}

namespace {

using xray::hexagon::model::AnalysisResult;
using xray::hexagon::model::ImpactedTarget;
using xray::hexagon::model::ImpactedTranslationUnit;
using xray::hexagon::model::ImpactKind;
using xray::hexagon::model::ImpactResult;
using xray::hexagon::model::IncludeHotspot;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::TargetAssignment;
using xray::hexagon::model::TargetDependency;
using xray::hexagon::model::TargetDependencyKind;
using xray::hexagon::model::TargetDependencyResolution;
using xray::hexagon::model::TargetGraph;
using xray::hexagon::model::TargetGraphStatus;
using xray::hexagon::model::TargetImpactClassification;
using xray::hexagon::model::TargetInfo;
using xray::hexagon::model::TranslationUnitReference;

constexpr std::size_t kLabelMaxLength = 48;
constexpr std::size_t kLabelHeadLength = 22;
constexpr std::size_t kLabelTailLength = 23;

// ---- Escaping and labels --------------------------------------------------

std::string escape_dot_string(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const unsigned char ch : value) {
        switch (ch) {
        case '\\':
            out.append("\\\\");
            break;
        case '"':
            out.append("\\\"");
            break;
        case '\n':
            out.append("\\n");
            break;
        case '\r':
            out.append("\\r");
            break;
        case '\t':
            out.append("\\t");
            break;
        default:
            if (ch < 0x20) {
                std::array<char, 5> buffer{};
                std::snprintf(buffer.data(), buffer.size(), "\\x%02X",
                              static_cast<unsigned>(ch));
                out.append(buffer.data());
            } else {
                out.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    // NRVO-defeating return mirrors render_impact_inputs in the JSON adapter
    // so the closing-brace destructor counts as a covered line under gcov.
    return std::string(std::move(out));
}

std::string truncate_label(std::string_view value) {
    if (value.size() <= kLabelMaxLength) {
        return std::string{value};
    }
    std::string out;
    out.reserve(kLabelMaxLength);
    out.append(value.substr(0, kLabelHeadLength));
    out.append("...");
    out.append(value.substr(value.size() - kLabelTailLength));
    return out;
}

std::string label_from_path(std::string_view path) {
    if (path.empty()) return std::string{};
    const auto last_separator = path.find_last_of("/\\");
    if (last_separator == std::string_view::npos) {
        return truncate_label(path);
    }
    if (last_separator + 1 >= path.size()) {
        return truncate_label(path);
    }
    return truncate_label(path.substr(last_separator + 1));
}

// ---- Attribute emitters ---------------------------------------------------

void append_string_attribute(std::ostringstream& out, std::string_view name,
                             std::string_view value) {
    out << name << "=\"" << escape_dot_string(value) << '"';
}

void append_integer_attribute(std::ostringstream& out, std::string_view name,
                              std::size_t value) {
    out << name << '=' << value;
}

void append_boolean_attribute(std::ostringstream& out, std::string_view name, bool value) {
    out << name << '=' << (value ? "true" : "false");
}

struct GraphHeader {
    std::string_view graph_name;
    std::string_view report_type;
    DotBudget budget;
    bool truncated;
    TargetGraphStatus target_graph_status{TargetGraphStatus::not_loaded};
    // AP M6-1.3 A.3: v3 graph-level depth attributes for impact reports.
    // emit_graph_header consults kReportFormatVersion via if constexpr;
    // in A.3 with kReportFormatVersion=2 the branch is statically
    // discarded so v2 reports stay byte-stable. A.4 flips the constant
    // and the attributes start appearing in impact-DOT output.
    std::size_t impact_target_depth_requested{0};
    std::size_t impact_target_depth_effective{0};
};

std::string target_graph_status_text(TargetGraphStatus status) {
    if (status == TargetGraphStatus::loaded) return "loaded";
    if (status == TargetGraphStatus::partial) return "partial";
    return "not_loaded";
}

void emit_graph_header(std::ostringstream& out, const GraphHeader& header) {
    out << "digraph " << header.graph_name << " {\n";
    out << "  ";
    append_string_attribute(out, "xray_report_type", header.report_type);
    out << ";\n";
    out << "  ";
    append_integer_attribute(out, "format_version",
                              static_cast<std::size_t>(
                                  xray::hexagon::model::kReportFormatVersion));
    out << ";\n";
    out << "  ";
    append_integer_attribute(out, "graph_node_limit", header.budget.node_limit);
    out << ";\n";
    out << "  ";
    append_integer_attribute(out, "graph_edge_limit", header.budget.edge_limit);
    out << ";\n";
    out << "  ";
    append_boolean_attribute(out, "graph_truncated", header.truncated);
    out << ";\n";
    out << "  ";
    append_string_attribute(out, "graph_target_graph_status",
                            target_graph_status_text(header.target_graph_status));
    out << ";\n";
    // AP M6-1.3 A.3: graph_impact_target_depth_{requested,effective} are
    // pflicht in Impact-DOT v3 (plan-M6-1-3.md "Neue Graph-Attribute fuer
    // Impact-DOT"). The if-constexpr is dead in A.3 (kReportFormatVersion=2)
    // and activates in A.4. Analyze-DOT does not carry these attributes.
    if constexpr (xray::hexagon::model::kReportFormatVersion >= 3) {
        if (header.report_type == "impact") {
            out << "  ";
            append_integer_attribute(out, "graph_impact_target_depth_requested",
                                      header.impact_target_depth_requested);
            out << ";\n";
            out << "  ";
            append_integer_attribute(out, "graph_impact_target_depth_effective",
                                      header.impact_target_depth_effective);
            out << ";\n";
        }
    }
}

void emit_graph_footer(std::ostringstream& out) { out << "}\n"; }

// ---- Sort helpers ---------------------------------------------------------

bool reference_less(const TranslationUnitReference& lhs,
                    const TranslationUnitReference& rhs) {
    if (lhs.source_path != rhs.source_path) return lhs.source_path < rhs.source_path;
    if (lhs.directory != rhs.directory) return lhs.directory < rhs.directory;
    return lhs.unique_key < rhs.unique_key;
}

bool target_less(const TargetInfo& lhs, const TargetInfo& rhs) {
    if (lhs.display_name != rhs.display_name) return lhs.display_name < rhs.display_name;
    if (lhs.type != rhs.type) return lhs.type < rhs.type;
    return lhs.unique_key < rhs.unique_key;
}

bool ranking_less(const RankedTranslationUnit& lhs, const RankedTranslationUnit& rhs) {
    if (lhs.rank != rhs.rank) return lhs.rank < rhs.rank;
    return reference_less(lhs.reference, rhs.reference);
}

bool hotspot_less(const IncludeHotspot& lhs, const IncludeHotspot& rhs) {
    const auto lhs_count = lhs.affected_translation_units.size();
    const auto rhs_count = rhs.affected_translation_units.size();
    if (lhs_count != rhs_count) return lhs_count > rhs_count;
    return lhs.header_path < rhs.header_path;
}

// ---- Analyze rendering ---------------------------------------------------

struct AnalyzeTuNode {
    std::string id;
    TranslationUnitReference reference;
    std::optional<std::size_t> rank;
    bool context_only{false};
};

struct AnalyzeHotspotNode {
    std::string id;
    std::string header_path;
    std::size_t context_total_count{0};
    std::size_t context_returned_count{0};
    bool context_truncated{false};
};

struct AnalyzeTargetNode {
    std::string id;
    TargetInfo target;
};

struct AnalyzeEdge {
    std::string source_id;
    std::string target_id;
    std::string_view kind;  // "tu_include_hotspot" or "tu_target"
};

// Synthetic <external>::* node placed at edge resolution time. Carries the raw
// File-API id so the adapter can emit the truncated label and the full
// external_target_id attribute side by side.
struct ExternalTargetNode {
    std::string id;          // external_1, external_2, ...
    std::string raw_id;      // text from <external>::<raw_id>
};

// Edge from a real target to either another real target (resolved) or to an
// ExternalTargetNode (external).
struct TargetDependencyEdge {
    std::string source_id;
    std::string target_id;
    TargetDependencyResolution resolution{TargetDependencyResolution::resolved};
    std::string raw_id;  // populated for external; empty for resolved.
};

struct AnalyzeGraph {
    std::vector<AnalyzeTuNode> tu_nodes;
    std::vector<AnalyzeHotspotNode> hotspot_nodes;
    std::vector<AnalyzeTargetNode> target_nodes;
    std::vector<ExternalTargetNode> external_nodes;
    std::vector<AnalyzeEdge> edges;
    std::vector<TargetDependencyEdge> dependency_edges;
    bool truncated{false};
};

// Carries the maps that build_* helpers populate during analyze rendering.
// Bundled into one struct so individual functions stay below the lizard
// parameter threshold and clang-tidy's easily-swappable-parameters check
// does not flag adjacent same-type maps.
struct AnalyzeContext {
    AnalyzeGraph& graph;
    const DotBudget& budget;
    std::map<std::string, std::string>& tu_id_by_key;
    std::map<std::string, std::string>& hotspot_id_by_path;
    std::map<std::string, std::string>& target_id_by_key;
    std::map<std::string, std::string>& context_id_by_key;
    std::vector<std::pair<std::string, std::string>>& context_pairs;
};

// In-place sort+truncate helpers; returning a sorted copy from a function
// would trigger NRVO and leave the closing brace as a missed line under gcov
// (same artifact the JSON adapter avoids by emitting from a temp expression).
void sort_top_n_ranking_in_place(std::vector<RankedTranslationUnit>& sorted,
                                  std::size_t top_limit) {
    std::stable_sort(sorted.begin(), sorted.end(), ranking_less);
    sorted.resize(std::min(top_limit, sorted.size()));
}

void sort_top_n_hotspots_in_place(std::vector<IncludeHotspot>& sorted,
                                   std::size_t top_limit) {
    std::stable_sort(sorted.begin(), sorted.end(), hotspot_less);
    sorted.resize(std::min(top_limit, sorted.size()));
}

void sort_context_refs_in_place(std::vector<TranslationUnitReference>& sorted) {
    std::stable_sort(sorted.begin(), sorted.end(), reference_less);
}

void collect_primary_targets_in_place(std::vector<TargetInfo>& sorted,
                                        const std::vector<RankedTranslationUnit>& primary_tus) {
    // Each primary TU may carry zero or more TargetInfo entries. Deduplicate
    // by unique_key and emit in display_name/type/unique_key sort order.
    std::map<std::string, TargetInfo> by_key;
    for (const auto& tu : primary_tus) {
        for (const auto& target : tu.targets) {
            by_key.emplace(target.unique_key, target);
        }
    }
    sorted.reserve(by_key.size());
    for (auto& [_, target] : by_key) sorted.push_back(std::move(target));
    std::stable_sort(sorted.begin(), sorted.end(), target_less);
}

bool try_add_node(AnalyzeGraph& graph, std::size_t node_limit, std::size_t* node_count) {
    if (*node_count >= node_limit) {
        graph.truncated = true;
        return false;
    }
    ++(*node_count);
    return true;
}

void build_analyze_nodes(AnalyzeContext& ctx,
                         const std::vector<RankedTranslationUnit>& primary,
                         const std::vector<IncludeHotspot>& hotspots,
                         const std::vector<TargetInfo>& targets) {
    std::size_t node_count = 0;
    std::size_t tu_index = 1;
    for (const auto& tu : primary) {
        if (!try_add_node(ctx.graph, ctx.budget.node_limit, &node_count)) return;
        AnalyzeTuNode node{};
        node.id = "tu_" + std::to_string(tu_index++);
        node.reference = tu.reference;
        node.rank = tu.rank;
        node.context_only = false;
        ctx.tu_id_by_key.emplace(tu.reference.unique_key, node.id);
        ctx.graph.tu_nodes.push_back(std::move(node));
    }
    std::size_t hotspot_index = 1;
    for (const auto& hotspot : hotspots) {
        if (!try_add_node(ctx.graph, ctx.budget.node_limit, &node_count)) return;
        AnalyzeHotspotNode node{};
        node.id = "hotspot_" + std::to_string(hotspot_index++);
        node.header_path = hotspot.header_path;
        node.context_total_count = hotspot.affected_translation_units.size();
        ctx.hotspot_id_by_path.emplace(hotspot.header_path, node.id);
        ctx.graph.hotspot_nodes.push_back(std::move(node));
    }
    std::size_t target_index = 1;
    for (const auto& target : targets) {
        if (!try_add_node(ctx.graph, ctx.budget.node_limit, &node_count)) return;
        AnalyzeTargetNode node{};
        node.id = "target_" + std::to_string(target_index++);
        node.target = target;
        ctx.target_id_by_key.emplace(target.unique_key, node.id);
        ctx.graph.target_nodes.push_back(std::move(node));
    }
}

void build_analyze_context_nodes(AnalyzeContext& ctx,
                                  const std::vector<IncludeHotspot>& hotspots) {
    // context_pairs collects (tu_unique_key, hotspot_header_path) ordered by
    // hotspot priority, then context-TU sort order. Edge generation will
    // iterate this list. context_id_by_key stores newly added context TUs
    // (i.e. TUs that are not already primary ranking nodes).
    std::size_t node_count = ctx.graph.tu_nodes.size() +
                              ctx.graph.hotspot_nodes.size() +
                              ctx.graph.target_nodes.size();
    std::size_t context_index = 1;
    for (const auto& hotspot : hotspots) {
        std::vector<TranslationUnitReference> sorted_refs{
            hotspot.affected_translation_units.begin(),
            hotspot.affected_translation_units.end()};
        sort_context_refs_in_place(sorted_refs);
        const auto take = std::min(ctx.budget.context_limit, sorted_refs.size());
        if (take < sorted_refs.size()) ctx.graph.truncated = true;
        for (std::size_t index = 0; index < take; ++index) {
            const auto& ref = sorted_refs[index];
            ctx.context_pairs.emplace_back(ref.unique_key, hotspot.header_path);
            if (ctx.tu_id_by_key.count(ref.unique_key) > 0) continue;
            if (ctx.context_id_by_key.count(ref.unique_key) > 0) continue;
            if (node_count >= ctx.budget.node_limit) {
                ctx.graph.truncated = true;
                continue;
            }
            ++node_count;
            AnalyzeTuNode node{};
            node.id = "context_tu_" + std::to_string(context_index++);
            node.reference = ref;
            node.context_only = true;
            ctx.context_id_by_key.emplace(ref.unique_key, node.id);
            ctx.graph.tu_nodes.push_back(std::move(node));
        }
    }
}

void try_add_analyze_edge(AnalyzeGraph& graph, std::size_t edge_limit,
                          std::string source_id, std::string target_id,
                          std::string_view kind) {
    if (graph.edges.size() >= edge_limit) {
        graph.truncated = true;
        return;
    }
    graph.edges.push_back({std::move(source_id), std::move(target_id), kind});
}

void build_analyze_edges(AnalyzeContext& ctx,
                          const std::vector<RankedTranslationUnit>& primary) {
    // Both phases delegate the edge_limit check to try_add_analyze_edge; the
    // loops run unconditionally and let the helper short-circuit each
    // attempt past the limit. This avoids two dead-code-detected early-exit
    // returns and keeps the truncation behaviour identical.
    for (const auto& [tu_key, hotspot_path] : ctx.context_pairs) {
        const auto src_primary = ctx.tu_id_by_key.find(tu_key);
        const auto src_context = ctx.context_id_by_key.find(tu_key);
        const auto dst = ctx.hotspot_id_by_path.find(hotspot_path);
        if (dst == ctx.hotspot_id_by_path.end()) continue;
        std::string src_id;
        if (src_primary != ctx.tu_id_by_key.end()) {
            src_id = src_primary->second;
        } else if (src_context != ctx.context_id_by_key.end()) {
            src_id = src_context->second;
        } else {
            ctx.graph.truncated = true;
            continue;
        }
        try_add_analyze_edge(ctx.graph, ctx.budget.edge_limit, src_id, dst->second,
                              "tu_include_hotspot");
    }

    for (const auto& tu : primary) {
        const auto src = ctx.tu_id_by_key.find(tu.reference.unique_key);
        if (src == ctx.tu_id_by_key.end()) continue;
        for (const auto& target : tu.targets) {
            const auto dst = ctx.target_id_by_key.find(target.unique_key);
            if (dst == ctx.target_id_by_key.end()) continue;
            try_add_analyze_edge(ctx.graph, ctx.budget.edge_limit, src->second,
                                 dst->second, "tu_target");
        }
    }
}

// Priority 5 in the M6 node-priority list: target nodes from
// AnalysisResult::target_graph.nodes that are not already part of the graph
// via priority 3 (M4 target_assignments). target_id_by_key is the shared
// dedup key, so the extra entries get the next available target_<index>.
void build_extra_target_nodes(AnalyzeContext& ctx, const TargetGraph& target_graph) {
    std::size_t node_count = ctx.graph.tu_nodes.size() +
                              ctx.graph.hotspot_nodes.size() +
                              ctx.graph.target_nodes.size() +
                              ctx.graph.external_nodes.size();
    std::size_t target_index = ctx.graph.target_nodes.size() + 1;
    for (const auto& node : target_graph.nodes) {
        if (ctx.target_id_by_key.count(node.unique_key) > 0) continue;
        if (node_count >= ctx.budget.node_limit) {
            ctx.graph.truncated = true;
            return;
        }
        ++node_count;
        AnalyzeTargetNode new_node{};
        new_node.id = "target_" + std::to_string(target_index++);
        new_node.target = node;
        ctx.target_id_by_key.emplace(node.unique_key, new_node.id);
        ctx.graph.target_nodes.push_back(std::move(new_node));
    }
}

struct TargetDependencyCandidate {
    std::string from_uk;
    std::string to_uk;
    TargetDependencyResolution resolution;
    std::string raw_id;  // populated for external; empty for resolved.
};

// Dedup target-graph edges by (from_uk, to_uk, kind) and sort by
// (from_uk, to_uk). Plan-M6-1-2.md only defines kind=direct for AP 1.2 so a
// single bucket per pair is sufficient; future kinds will require an
// explicit kind tier in the bucket key.
std::map<std::pair<std::string, std::string>, TargetDependencyCandidate>
dedup_target_dependency_candidates(const TargetGraph& target_graph) {
    std::map<std::pair<std::string, std::string>, TargetDependencyCandidate> by_pair;
    for (const auto& edge : target_graph.edges) {
        if (edge.kind != TargetDependencyKind::direct) continue;
        const auto key = std::make_pair(edge.from_unique_key, edge.to_unique_key);
        by_pair.try_emplace(
            key, TargetDependencyCandidate{
                     edge.from_unique_key, edge.to_unique_key, edge.resolution,
                     edge.resolution == TargetDependencyResolution::external
                         ? edge.to_display_name
                         : std::string{}});
    }
    // Move-construct return defeats NRVO so the closing brace is a covered line.
    return std::move(by_pair);
}

// Filter dependency candidates to those whose source target survived
// build_extra_target_nodes (target_id_by_key) and, for resolved edges, whose
// destination target also survived. External edges defer the dst-existence
// check to allocate_external_target_nodes via the surviving_external map.
template <typename Context>
std::vector<TargetDependencyCandidate> filter_resolvable_candidates(
    Context& ctx,
    const std::map<std::pair<std::string, std::string>, TargetDependencyCandidate>&
        by_pair) {
    std::vector<TargetDependencyCandidate> candidates;
    candidates.reserve(by_pair.size());
    for (const auto& [_, c] : by_pair) {
        if (ctx.target_id_by_key.count(c.from_uk) == 0) {
            ctx.graph.truncated = true;
            continue;
        }
        if (c.resolution == TargetDependencyResolution::resolved &&
            ctx.target_id_by_key.count(c.to_uk) == 0) {
            ctx.graph.truncated = true;
            continue;
        }
        candidates.push_back(c);
    }
    return std::move(candidates);
}

// Priority 6 in the M6 node-priority list: synthetic external_<index> nodes
// for `<external>::*` destinations. Index assignment is a pure function of
// the sorted to_unique_key set, so analyze and impact rendering of the same
// external set produce byte-identical IDs.
void allocate_external_target_nodes(
    AnalyzeContext& ctx,
    const std::vector<TargetDependencyCandidate>& candidates,
    std::map<std::string, std::string>& external_id_by_to_uk) {
    std::map<std::string, std::string> ext_to_raw;
    for (const auto& c : candidates) {
        if (c.resolution != TargetDependencyResolution::external) continue;
        ext_to_raw.emplace(c.to_uk, c.raw_id);
    }
    std::size_t node_count = ctx.graph.tu_nodes.size() +
                              ctx.graph.hotspot_nodes.size() +
                              ctx.graph.target_nodes.size() +
                              ctx.graph.external_nodes.size();
    std::size_t external_index = ctx.graph.external_nodes.size() + 1;
    for (const auto& [to_uk, raw_id] : ext_to_raw) {
        if (node_count >= ctx.budget.node_limit) {
            ctx.graph.truncated = true;
            continue;  // surviving edges to this destination get filtered later.
        }
        ++node_count;
        ExternalTargetNode node;
        node.id = "external_" + std::to_string(external_index++);
        node.raw_id = raw_id;
        external_id_by_to_uk.emplace(to_uk, node.id);
        ctx.graph.external_nodes.push_back(std::move(node));
    }
}

// Edge priority 3: append target_dependency edges after tu_include_hotspot
// (priority 1) and tu_target (priority 2). The shared edge_limit budget is
// counted across edges + dependency_edges so M6 edges yield first under
// pressure per plan-M6-1-2.md.
void build_target_dependency_edges(AnalyzeContext& ctx,
                                    const TargetGraph& target_graph) {
    const auto by_pair = dedup_target_dependency_candidates(target_graph);
    const auto candidates = filter_resolvable_candidates(ctx, by_pair);
    std::map<std::string, std::string> external_id_by_to_uk;
    allocate_external_target_nodes(ctx, candidates, external_id_by_to_uk);

    for (const auto& c : candidates) {
        std::string dst_id;
        if (c.resolution == TargetDependencyResolution::resolved) {
            // filter_resolvable_candidates already verified target_id_by_key
            // has c.to_uk; use at() so a future regression surfaces as a
            // throw rather than silent edge drop.
            dst_id = ctx.target_id_by_key.at(c.to_uk);
        } else {
            const auto dst_it = external_id_by_to_uk.find(c.to_uk);
            if (dst_it == external_id_by_to_uk.end()) {
                ctx.graph.truncated = true;
                continue;
            }
            dst_id = dst_it->second;
        }
        const auto total_edges =
            ctx.graph.edges.size() + ctx.graph.dependency_edges.size();
        if (total_edges >= ctx.budget.edge_limit) {
            ctx.graph.truncated = true;
            continue;
        }
        ctx.graph.dependency_edges.push_back(
            {ctx.target_id_by_key.at(c.from_uk), dst_id, c.resolution, c.raw_id});
    }
}

void remove_orphan_context_nodes(AnalyzeGraph& graph) {
    std::unordered_set<std::string> referenced;
    for (const auto& edge : graph.edges) {
        referenced.insert(edge.source_id);
        referenced.insert(edge.target_id);
    }
    auto removed_at = std::remove_if(
        graph.tu_nodes.begin(), graph.tu_nodes.end(),
        [&referenced](const AnalyzeTuNode& node) {
            return node.context_only && referenced.count(node.id) == 0;
        });
    // erase(end, end) is a no-op so the unconditional call covers both the
    // "orphans found" and "no orphans" paths.
    graph.tu_nodes.erase(removed_at, graph.tu_nodes.end());
}

void compute_hotspot_context_counts(AnalyzeGraph& graph) {
    for (auto& hotspot : graph.hotspot_nodes) {
        std::unordered_set<std::string> connected_tus;
        for (const auto& edge : graph.edges) {
            if (edge.kind != std::string_view{"tu_include_hotspot"}) continue;
            if (edge.target_id != hotspot.id) continue;
            connected_tus.insert(edge.source_id);
        }
        hotspot.context_returned_count = connected_tus.size();
        hotspot.context_truncated =
            hotspot.context_total_count > hotspot.context_returned_count;
        if (hotspot.context_truncated) graph.truncated = true;
    }
}

void emit_analyze_tu_node(std::ostringstream& out, const AnalyzeTuNode& node) {
    out << "  " << node.id << " [";
    append_string_attribute(out, "kind", "translation_unit");
    if (node.rank.has_value()) {
        out << ", ";
        append_integer_attribute(out, "rank", *node.rank);
    }
    if (node.context_only) {
        out << ", ";
        append_boolean_attribute(out, "context_only", true);
    }
    out << ", ";
    append_string_attribute(out, "label", label_from_path(node.reference.source_path));
    out << ", ";
    append_string_attribute(out, "path", node.reference.source_path);
    out << ", ";
    append_string_attribute(out, "directory", node.reference.directory);
    out << ", ";
    append_string_attribute(out, "unique_key", node.reference.unique_key);
    out << "];\n";
}

void emit_analyze_hotspot_node(std::ostringstream& out, const AnalyzeHotspotNode& node) {
    out << "  " << node.id << " [";
    append_string_attribute(out, "kind", "include_hotspot");
    out << ", ";
    append_string_attribute(out, "label", label_from_path(node.header_path));
    out << ", ";
    append_string_attribute(out, "path", node.header_path);
    out << ", ";
    append_integer_attribute(out, "context_total_count", node.context_total_count);
    out << ", ";
    append_integer_attribute(out, "context_returned_count", node.context_returned_count);
    out << ", ";
    append_boolean_attribute(out, "context_truncated", node.context_truncated);
    out << "];\n";
}

void emit_analyze_target_node(std::ostringstream& out, const AnalyzeTargetNode& node) {
    out << "  " << node.id << " [";
    append_string_attribute(out, "kind", "target");
    out << ", ";
    append_string_attribute(out, "label", node.target.display_name);
    out << ", ";
    append_string_attribute(out, "name", node.target.display_name);
    out << ", ";
    append_string_attribute(out, "type", node.target.type);
    out << ", ";
    append_string_attribute(out, "unique_key", node.target.unique_key);
    out << "];\n";
}

void emit_external_target_node(std::ostringstream& out,
                               const ExternalTargetNode& node) {
    out << "  " << node.id << " [";
    append_string_attribute(out, "kind", "external_target");
    out << ", ";
    // label gets middle-truncation per docs/planning/plan-M6-1-2.md DOT-Vertragsregeln;
    // append_string_attribute then escapes the truncated value as a DOT string.
    append_string_attribute(out, "label", truncate_label(node.raw_id));
    out << ", ";
    // external_target_id stays full-length so the raw id is recoverable even
    // if the label was truncated. Escape contract still applies.
    append_string_attribute(out, "external_target_id", node.raw_id);
    out << "];\n";
}

void emit_analyze_edge(std::ostringstream& out, const AnalyzeEdge& edge) {
    out << "  " << edge.source_id << " -> " << edge.target_id << " [";
    append_string_attribute(out, "kind", edge.kind);
    out << "];\n";
}

void emit_target_dependency_edge(std::ostringstream& out,
                                 const TargetDependencyEdge& edge) {
    const bool is_external = edge.resolution == TargetDependencyResolution::external;
    out << "  " << edge.source_id << " -> " << edge.target_id << " [";
    append_string_attribute(out, "kind", "target_dependency");
    out << ", ";
    append_string_attribute(out, "style", is_external ? "dashed" : "solid");
    out << ", ";
    append_string_attribute(out, "resolution", is_external ? "external" : "resolved");
    if (is_external) {
        out << ", ";
        append_string_attribute(out, "external_target_id", edge.raw_id);
    }
    out << "];\n";
}

void emit_analyze_graph(std::ostringstream& out, const AnalyzeGraph& graph,
                        const DotBudget& budget,
                        TargetGraphStatus target_graph_status) {
    emit_graph_header(out, GraphHeader{"cmake_xray_analysis", "analyze", budget,
                                       graph.truncated, target_graph_status});
    for (const auto& node : graph.tu_nodes) emit_analyze_tu_node(out, node);
    for (const auto& node : graph.hotspot_nodes) emit_analyze_hotspot_node(out, node);
    for (const auto& node : graph.target_nodes) emit_analyze_target_node(out, node);
    for (const auto& node : graph.external_nodes) emit_external_target_node(out, node);
    for (const auto& edge : graph.edges) emit_analyze_edge(out, edge);
    for (const auto& edge : graph.dependency_edges)
        emit_target_dependency_edge(out, edge);
    emit_graph_footer(out);
}

std::string render_analyze(const AnalysisResult& result, std::size_t top_limit) {
    const auto budget = compute_analyze_budget(top_limit);
    std::vector<RankedTranslationUnit> primary{result.translation_units.begin(),
                                                 result.translation_units.end()};
    sort_top_n_ranking_in_place(primary, top_limit);
    std::vector<IncludeHotspot> top_hotspots{result.include_hotspots.begin(),
                                               result.include_hotspots.end()};
    sort_top_n_hotspots_in_place(top_hotspots, top_limit);
    std::vector<TargetInfo> target_candidates;
    collect_primary_targets_in_place(target_candidates, primary);

    AnalyzeGraph graph;
    std::map<std::string, std::string> tu_id_by_key;
    std::map<std::string, std::string> hotspot_id_by_path;
    std::map<std::string, std::string> target_id_by_key;
    std::map<std::string, std::string> context_id_by_key;
    std::vector<std::pair<std::string, std::string>> context_pairs;
    AnalyzeContext ctx{graph,           budget,            tu_id_by_key,
                       hotspot_id_by_path, target_id_by_key, context_id_by_key,
                       context_pairs};

    build_analyze_nodes(ctx, primary, top_hotspots, target_candidates);
    build_analyze_context_nodes(ctx, top_hotspots);
    build_extra_target_nodes(ctx, result.target_graph);
    build_analyze_edges(ctx, primary);
    build_target_dependency_edges(ctx, result.target_graph);
    remove_orphan_context_nodes(graph);
    compute_hotspot_context_counts(graph);

    std::ostringstream out;
    emit_analyze_graph(out, graph, budget, result.target_graph_status);
    return out.str();
}

// ---- Impact rendering ----------------------------------------------------

struct ImpactTuNode {
    std::string id;
    TranslationUnitReference reference;
    ImpactKind kind{ImpactKind::direct};
};

struct ImpactTargetNode {
    std::string id;
    TargetInfo target;
    bool direct{false};  // direct wins over heuristic on dual-membership.
    // True when the node carries an impact classification (from
    // ImpactResult::affected_targets). False for nodes that exist only
    // because target_graph.nodes propagated them; those are reference data
    // and must not advertise an impact attribute per spec/report-dot.md
    // (impact is optional on target nodes).
    bool has_impact_attribute{true};
    // AP M6-1.3 A.3: v3 priority attributes carried alongside the M5
    // impact classification. Filled by enrich_impact_target_nodes_with_priorities
    // when the result has a usable target graph and a matching
    // prioritized_affected_targets entry; emitted by emit_impact_target_node
    // only when kReportFormatVersion >= 3 (A.4 flips the constant). In
    // A.3 these stay default-initialized and unused.
    std::string priority_class_text;
    std::string evidence_strength_text;
    std::size_t graph_distance{0};
    bool has_priority_attributes{false};
};

struct ImpactEdge {
    std::string source_id;
    std::string target_id;
    std::string_view kind;
    std::string_view style;
};

struct ImpactGraph {
    bool has_changed_file{false};
    std::string changed_file_path;
    std::vector<ImpactTuNode> tu_nodes;
    std::vector<ImpactTargetNode> target_nodes;
    std::vector<ExternalTargetNode> external_nodes;
    std::vector<ImpactEdge> edges;
    std::vector<TargetDependencyEdge> dependency_edges;
    bool truncated{false};
};

// Mirror of AnalyzeContext for impact rendering.
struct ImpactContext {
    ImpactGraph& graph;
    const DotBudget& budget;
    std::map<std::string, std::string>& direct_tu_id_by_key;
    std::map<std::string, std::string>& heuristic_tu_id_by_key;
    std::map<std::string, std::string>& target_id_by_key;
};

bool impacted_tu_less(const ImpactedTranslationUnit& lhs,
                      const ImpactedTranslationUnit& rhs) {
    return reference_less(lhs.reference, rhs.reference);
}

void filter_and_sort_impacted_tus_in_place(
    std::vector<ImpactedTranslationUnit>& filtered,
    const std::vector<ImpactedTranslationUnit>& units, ImpactKind wanted) {
    for (const auto& tu : units) {
        if (tu.kind == wanted) filtered.push_back(tu);
    }
    std::stable_sort(filtered.begin(), filtered.end(), impacted_tu_less);
}

void collect_impact_targets_in_place(std::vector<ImpactTargetNode>& sorted,
                                      const std::vector<ImpactedTarget>& impacted) {
    std::map<std::string, ImpactTargetNode> by_key;
    for (const auto& it : impacted) {
        const auto direct = it.classification == TargetImpactClassification::direct;
        auto [iter, inserted] = by_key.emplace(it.target.unique_key,
                                               ImpactTargetNode{{}, it.target, direct});
        if (!inserted && direct) iter->second.direct = true;
    }
    sorted.reserve(by_key.size());
    for (auto& [_, node] : by_key) sorted.push_back(std::move(node));
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const ImpactTargetNode& lhs, const ImpactTargetNode& rhs) {
                         return target_less(lhs.target, rhs.target);
                     });
}

bool try_add_impact_node(ImpactGraph& graph, std::size_t node_limit,
                         std::size_t* node_count) {
    if (*node_count >= node_limit) {
        graph.truncated = true;
        return false;
    }
    ++(*node_count);
    return true;
}

struct ImpactTuNodeSpec {
    std::string_view id_prefix;
    ImpactKind kind;
    // reference_wrapper instead of a raw pointer makes lifetime explicit and
    // prevents accidental null-deref under future edits; aggregate
    // initialization stays just as terse as the pointer form.
    std::reference_wrapper<std::map<std::string, std::string>> id_lookup;
};

void build_impact_tu_nodes(ImpactContext& ctx,
                            const std::vector<ImpactedTranslationUnit>& units,
                            const ImpactTuNodeSpec& spec, std::size_t* node_count) {
    std::size_t index = 1;
    for (const auto& tu : units) {
        if (!try_add_impact_node(ctx.graph, ctx.budget.node_limit, node_count)) return;
        ImpactTuNode node{};
        node.id = std::string{spec.id_prefix} + std::to_string(index++);
        node.reference = tu.reference;
        node.kind = spec.kind;
        spec.id_lookup.get().emplace(tu.reference.unique_key, node.id);
        ctx.graph.tu_nodes.push_back(std::move(node));
    }
}

void build_impact_nodes(ImpactContext& ctx, const ImpactResult& result) {
    std::size_t node_count = 0;
    if (result.inputs.changed_file.has_value()) {
        if (!try_add_impact_node(ctx.graph, ctx.budget.node_limit, &node_count)) return;
        ctx.graph.has_changed_file = true;
        ctx.graph.changed_file_path = *result.inputs.changed_file;
    }
    std::vector<ImpactedTranslationUnit> direct;
    filter_and_sort_impacted_tus_in_place(direct, result.affected_translation_units,
                                            ImpactKind::direct);
    build_impact_tu_nodes(ctx, direct,
                           {"direct_tu_", ImpactKind::direct,
                            std::ref(ctx.direct_tu_id_by_key)},
                           &node_count);
    if (ctx.graph.truncated) return;
    std::vector<ImpactedTranslationUnit> heuristic;
    filter_and_sort_impacted_tus_in_place(heuristic, result.affected_translation_units,
                                            ImpactKind::heuristic);
    build_impact_tu_nodes(ctx, heuristic,
                           {"heuristic_tu_", ImpactKind::heuristic,
                            std::ref(ctx.heuristic_tu_id_by_key)},
                           &node_count);
    if (ctx.graph.truncated) return;
    std::vector<ImpactTargetNode> targets;
    collect_impact_targets_in_place(targets, result.affected_targets);
    std::size_t target_index = 1;
    for (auto& node : targets) {
        if (!try_add_impact_node(ctx.graph, ctx.budget.node_limit, &node_count)) return;
        node.id = "target_" + std::to_string(target_index++);
        ctx.target_id_by_key.emplace(node.target.unique_key, node.id);
        ctx.graph.target_nodes.push_back(std::move(node));
    }
}

void try_add_impact_edge(ImpactContext& ctx, std::string source_id,
                          std::string target_id, std::string_view kind,
                          std::string_view style) {
    if (ctx.graph.edges.size() >= ctx.budget.edge_limit) {
        ctx.graph.truncated = true;
        return;
    }
    ctx.graph.edges.push_back({std::move(source_id), std::move(target_id), kind, style});
}

void emit_changed_file_edges(ImpactContext& ctx,
                              const std::vector<ImpactedTranslationUnit>& tus,
                              const std::map<std::string, std::string>& id_lookup,
                              std::string_view kind, std::string_view style) {
    const std::string changed_file_id = "changed_file";
    for (const auto& tu : tus) {
        const auto it = id_lookup.find(tu.reference.unique_key);
        if (it == id_lookup.end()) continue;
        try_add_impact_edge(ctx, changed_file_id, it->second, kind, style);
    }
}

void emit_tu_target_edges(ImpactContext& ctx,
                           const std::vector<ImpactedTranslationUnit>& tus,
                           const std::map<std::string, std::string>& id_lookup,
                           std::string_view kind, std::string_view style) {
    for (const auto& tu : tus) {
        const auto src = id_lookup.find(tu.reference.unique_key);
        if (src == id_lookup.end()) continue;
        for (const auto& target : tu.targets) {
            const auto dst = ctx.target_id_by_key.find(target.unique_key);
            if (dst == ctx.target_id_by_key.end()) continue;
            try_add_impact_edge(ctx, src->second, dst->second, kind, style);
        }
    }
}

void build_impact_edges(ImpactContext& ctx, const ImpactResult& result) {
    if (!ctx.graph.has_changed_file) return;
    std::vector<ImpactedTranslationUnit> direct_tus;
    filter_and_sort_impacted_tus_in_place(direct_tus, result.affected_translation_units,
                                            ImpactKind::direct);
    std::vector<ImpactedTranslationUnit> heuristic_tus;
    filter_and_sort_impacted_tus_in_place(heuristic_tus, result.affected_translation_units,
                                            ImpactKind::heuristic);
    emit_changed_file_edges(ctx, direct_tus, ctx.direct_tu_id_by_key,
                              "changed_file_direct_tu", "solid");
    emit_changed_file_edges(ctx, heuristic_tus, ctx.heuristic_tu_id_by_key,
                              "changed_file_heuristic_tu", "dashed");
    emit_tu_target_edges(ctx, direct_tus, ctx.direct_tu_id_by_key, "direct_tu_target",
                          "solid");
    emit_tu_target_edges(ctx, heuristic_tus, ctx.heuristic_tu_id_by_key,
                          "heuristic_tu_target", "dashed");
}

// Impact-side mirror of the analyze priority-5 step: append target nodes from
// ImpactResult::target_graph.nodes that are not already in target_id_by_key
// (which holds the existing M4-impact target nodes from
// ImpactResult::affected_targets).
void build_extra_impact_target_nodes(ImpactContext& ctx,
                                      const TargetGraph& target_graph) {
    std::size_t node_count = (ctx.graph.has_changed_file ? 1 : 0) +
                              ctx.graph.tu_nodes.size() +
                              ctx.graph.target_nodes.size() +
                              ctx.graph.external_nodes.size();
    std::size_t target_index = ctx.graph.target_nodes.size() + 1;
    for (const auto& node : target_graph.nodes) {
        if (ctx.target_id_by_key.count(node.unique_key) > 0) continue;
        if (node_count >= ctx.budget.node_limit) {
            ctx.graph.truncated = true;
            return;
        }
        ++node_count;
        ImpactTargetNode new_node{};
        new_node.id = "target_" + std::to_string(target_index++);
        new_node.target = node;
        new_node.has_impact_attribute = false;
        ctx.target_id_by_key.emplace(node.unique_key, new_node.id);
        ctx.graph.target_nodes.push_back(std::move(new_node));
    }
}

void allocate_impact_external_target_nodes(
    ImpactContext& ctx,
    const std::vector<TargetDependencyCandidate>& candidates,
    std::map<std::string, std::string>& external_id_by_to_uk) {
    std::map<std::string, std::string> ext_to_raw;
    for (const auto& c : candidates) {
        if (c.resolution != TargetDependencyResolution::external) continue;
        ext_to_raw.emplace(c.to_uk, c.raw_id);
    }
    std::size_t node_count = (ctx.graph.has_changed_file ? 1 : 0) +
                              ctx.graph.tu_nodes.size() +
                              ctx.graph.target_nodes.size() +
                              ctx.graph.external_nodes.size();
    std::size_t external_index = ctx.graph.external_nodes.size() + 1;
    for (const auto& [to_uk, raw_id] : ext_to_raw) {
        if (node_count >= ctx.budget.node_limit) {
            ctx.graph.truncated = true;
            continue;
        }
        ++node_count;
        ExternalTargetNode node;
        node.id = "external_" + std::to_string(external_index++);
        node.raw_id = raw_id;
        external_id_by_to_uk.emplace(to_uk, node.id);
        ctx.graph.external_nodes.push_back(std::move(node));
    }
}

void build_impact_target_dependency_edges(ImpactContext& ctx,
                                          const TargetGraph& target_graph) {
    const auto by_pair = dedup_target_dependency_candidates(target_graph);
    const auto candidates = filter_resolvable_candidates(ctx, by_pair);
    std::map<std::string, std::string> external_id_by_to_uk;
    allocate_impact_external_target_nodes(ctx, candidates, external_id_by_to_uk);

    for (const auto& c : candidates) {
        std::string dst_id;
        if (c.resolution == TargetDependencyResolution::resolved) {
            dst_id = ctx.target_id_by_key.at(c.to_uk);
        } else {
            const auto dst_it = external_id_by_to_uk.find(c.to_uk);
            if (dst_it == external_id_by_to_uk.end()) {
                ctx.graph.truncated = true;
                continue;
            }
            dst_id = dst_it->second;
        }
        const auto total_edges =
            ctx.graph.edges.size() + ctx.graph.dependency_edges.size();
        if (total_edges >= ctx.budget.edge_limit) {
            ctx.graph.truncated = true;
            continue;
        }
        ctx.graph.dependency_edges.push_back(
            {ctx.target_id_by_key.at(c.from_uk), dst_id, c.resolution, c.raw_id});
    }
}

void emit_impact_changed_file_node(std::ostringstream& out, std::string_view path) {
    out << "  changed_file [";
    append_string_attribute(out, "kind", "changed_file");
    out << ", ";
    append_string_attribute(out, "label", label_from_path(path));
    out << ", ";
    append_string_attribute(out, "path", path);
    out << "];\n";
}

void emit_impact_tu_node(std::ostringstream& out, const ImpactTuNode& node) {
    out << "  " << node.id << " [";
    append_string_attribute(out, "kind", "translation_unit");
    out << ", ";
    append_string_attribute(out, "impact",
                             node.kind == ImpactKind::direct ? "direct" : "heuristic");
    out << ", ";
    append_string_attribute(out, "label", label_from_path(node.reference.source_path));
    out << ", ";
    append_string_attribute(out, "path", node.reference.source_path);
    out << ", ";
    append_string_attribute(out, "directory", node.reference.directory);
    out << ", ";
    append_string_attribute(out, "unique_key", node.reference.unique_key);
    out << "];\n";
}

void emit_impact_target_node(std::ostringstream& out, const ImpactTargetNode& node) {
    out << "  " << node.id << " [";
    append_string_attribute(out, "kind", "target");
    if (node.has_impact_attribute) {
        out << ", ";
        append_string_attribute(out, "impact", node.direct ? "direct" : "heuristic");
    }
    // AP M6-1.3 A.3: v3 priority attributes on impact target nodes.
    // Plan-M6-1-3.md "Diese Attribute erscheinen ausschliesslich an
    // Target-Knoten in der Impact-Sicht ... und nur wenn
    // target_graph_status != not_loaded". The if-constexpr is dead in
    // A.3 (kReportFormatVersion=2); A.4 flips the constant.
    if constexpr (xray::hexagon::model::kReportFormatVersion >= 3) {
        if (node.has_priority_attributes) {
            out << ", ";
            append_string_attribute(out, "priority_class", node.priority_class_text);
            out << ", ";
            append_integer_attribute(out, "graph_distance", node.graph_distance);
            out << ", ";
            append_string_attribute(out, "evidence_strength", node.evidence_strength_text);
        }
    }
    out << ", ";
    append_string_attribute(out, "label", node.target.display_name);
    out << ", ";
    append_string_attribute(out, "name", node.target.display_name);
    out << ", ";
    append_string_attribute(out, "type", node.target.type);
    out << ", ";
    append_string_attribute(out, "unique_key", node.target.unique_key);
    out << "];\n";
}

void emit_impact_edge(std::ostringstream& out, const ImpactEdge& edge) {
    out << "  " << edge.source_id << " -> " << edge.target_id << " [";
    append_string_attribute(out, "kind", edge.kind);
    out << ", ";
    append_string_attribute(out, "style", edge.style);
    out << "];\n";
}

struct ImpactDepthFields {
    std::size_t impact_target_depth_requested{0};
    std::size_t impact_target_depth_effective{0};
};

void emit_impact_graph(std::ostringstream& out, const ImpactGraph& graph,
                       const DotBudget& budget,
                       TargetGraphStatus target_graph_status,
                       ImpactDepthFields depth) {
    emit_graph_header(
        out,
        GraphHeader{"cmake_xray_impact", "impact", budget, graph.truncated,
                    target_graph_status, depth.impact_target_depth_requested,
                    depth.impact_target_depth_effective});
    if (graph.has_changed_file) {
        emit_impact_changed_file_node(out, graph.changed_file_path);
    }
    for (const auto& node : graph.tu_nodes) emit_impact_tu_node(out, node);
    for (const auto& node : graph.target_nodes) emit_impact_target_node(out, node);
    for (const auto& node : graph.external_nodes) emit_external_target_node(out, node);
    for (const auto& edge : graph.edges) emit_impact_edge(out, edge);
    for (const auto& edge : graph.dependency_edges)
        emit_target_dependency_edge(out, edge);
    emit_graph_footer(out);
}

// AP M6-1.3 A.3: join the prioritised-targets data onto the impact
// target nodes so emit_impact_target_node can attach the v3 attributes
// per plan-M6-1-3.md "Neue Knotenattribute". The function runs always,
// but the resulting fields are only emitted when kReportFormatVersion
// >= 3; in A.3 they stay dormant.
void enrich_impact_target_nodes_with_priorities(
    std::vector<ImpactTargetNode>& nodes,
    const std::vector<xray::hexagon::model::PrioritizedImpactedTarget>& priorities,
    TargetGraphStatus status) {
    if (status == TargetGraphStatus::not_loaded) return;
    std::map<std::string, const xray::hexagon::model::PrioritizedImpactedTarget*>
        by_key;
    for (const auto& priority : priorities) {
        by_key.emplace(priority.target.unique_key, &priority);
    }
    for (auto& node : nodes) {
        const auto it = by_key.find(node.target.unique_key);
        if (it == by_key.end()) continue;
        node.priority_class_text = priority_class_text_v3(it->second->priority_class);
        node.evidence_strength_text =
            evidence_strength_text_v3(it->second->evidence_strength);
        node.graph_distance = it->second->graph_distance;
        node.has_priority_attributes = true;
    }
}

std::string render_impact(const ImpactResult& result) {
    const auto budget = compute_impact_budget();
    ImpactGraph graph;
    std::map<std::string, std::string> direct_tu_id_by_key;
    std::map<std::string, std::string> heuristic_tu_id_by_key;
    std::map<std::string, std::string> target_id_by_key;
    ImpactContext ctx{graph, budget, direct_tu_id_by_key, heuristic_tu_id_by_key,
                       target_id_by_key};
    build_impact_nodes(ctx, result);
    build_impact_edges(ctx, result);
    build_extra_impact_target_nodes(ctx, result.target_graph);
    build_impact_target_dependency_edges(ctx, result.target_graph);
    enrich_impact_target_nodes_with_priorities(
        graph.target_nodes, result.prioritized_affected_targets,
        result.target_graph_status);
    std::ostringstream out;
    emit_impact_graph(out, graph, budget, result.target_graph_status,
                       ImpactDepthFields{result.impact_target_depth_requested,
                                         result.impact_target_depth_effective});
    return out.str();
}

}  // namespace

std::string DotReportAdapter::write_analysis_report(
    const xray::hexagon::model::AnalysisResult& analysis_result,
    std::size_t top_limit) const {
    return render_analyze(analysis_result, top_limit);
}

std::string DotReportAdapter::write_impact_report(
    const xray::hexagon::model::ImpactResult& impact_result) const {
    return render_impact(impact_result);
}

}  // namespace xray::adapters::output
