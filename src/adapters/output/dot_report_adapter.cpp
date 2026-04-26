#include "adapters/output/dot_report_adapter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "hexagon/model/analysis_result.h"
#include "hexagon/model/impact_result.h"
#include "hexagon/model/report_format_version.h"
#include "hexagon/model/translation_unit.h"

namespace xray::adapters::output {

DotBudget compute_analyze_budget(std::size_t top_limit) {
    // docs/report-dot.md: context_limit = min(top_limit, 5);
    //                    node_limit = max(25, 4 * top_limit + 10);
    //                    edge_limit = max(40, 6 * top_limit + 20).
    DotBudget budget{};
    budget.context_limit = top_limit < 5 ? top_limit : 5;
    const auto node_candidate = 4 * top_limit + 10;
    budget.node_limit = node_candidate < 25 ? std::size_t{25} : node_candidate;
    const auto edge_candidate = 6 * top_limit + 20;
    budget.edge_limit = edge_candidate < 40 ? std::size_t{40} : edge_candidate;
    return budget;
}

DotBudget compute_impact_budget() {
    // docs/report-dot.md: fixed M5 budgets, node_limit = 100, edge_limit = 200.
    DotBudget budget{};
    budget.context_limit = 0;
    budget.node_limit = 100;
    budget.edge_limit = 200;
    return budget;
}

namespace {

// Tranche A scope: emit graph header plus minimal node-only rendering for
// analyze (translation unit nodes) and impact (changed_file node). Tranche B
// adds hotspots, targets, edges, context expansion, and budget enforcement.
// The minimal render here is enough to exercise escape_dot_string,
// truncate_label, and label_from_path through the public adapter so the
// contract gates and coverage stay green.

constexpr std::size_t kLabelMaxLength = 48;
constexpr std::size_t kLabelHeadLength = 22;
constexpr std::size_t kLabelTailLength = 23;

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
    // Construct a fresh std::string from out instead of returning out
    // directly. NRVO would otherwise elide the local destructor at the
    // function-end brace, which the project's 100%-coverage gate flags as
    // a missed line (same pattern as the JSON adapter's render_impact_inputs).
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
};

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
}

void emit_graph_footer(std::ostringstream& out) { out << "}\n"; }

void emit_translation_unit_node(std::ostringstream& out, std::size_t index,
                                const xray::hexagon::model::TranslationUnitReference& ref) {
    // Tranche A: minimal node emission; Tranche B adds rank/context_only/impact.
    out << "  tu_" << index << " [";
    append_string_attribute(out, "kind", "translation_unit");
    out << ", ";
    append_string_attribute(out, "label", label_from_path(ref.source_path));
    out << ", ";
    append_string_attribute(out, "path", ref.source_path);
    out << ", ";
    append_string_attribute(out, "directory", ref.directory);
    out << ", ";
    append_string_attribute(out, "unique_key", ref.unique_key);
    out << "];\n";
}

void emit_changed_file_node(std::ostringstream& out, std::string_view path) {
    out << "  changed_file [";
    append_string_attribute(out, "kind", "changed_file");
    out << ", ";
    append_string_attribute(out, "label", label_from_path(path));
    out << ", ";
    append_string_attribute(out, "path", path);
    out << "];\n";
}

std::string render_analyze(const xray::hexagon::model::AnalysisResult& result,
                           std::size_t top_limit) {
    std::ostringstream out;
    emit_graph_header(out, GraphHeader{"cmake_xray_analysis", "analyze",
                                       compute_analyze_budget(top_limit), false});
    // Tranche A: emit only translation-unit nodes from the result, in model
    // order, without enforcing the analyze ranking sort or the node budget.
    // Tranche B replaces this with the full priority-ordered, budget-bounded
    // renderer (ranking + hotspots + targets + context + edges).
    std::size_t index = 1;
    for (const auto& tu : result.translation_units) {
        emit_translation_unit_node(out, index, tu.reference);
        ++index;
    }
    emit_graph_footer(out);
    return out.str();
}

std::string render_impact(const xray::hexagon::model::ImpactResult& result) {
    std::ostringstream out;
    emit_graph_header(out, GraphHeader{"cmake_xray_impact", "impact",
                                       compute_impact_budget(), false});
    // Tranche A: emit only the changed-file node when present. Tranche B
    // adds direct/heuristic translation-unit nodes, target nodes, edges, and
    // the fixed Impact budget enforcement.
    if (result.inputs.changed_file.has_value()) {
        emit_changed_file_node(out, *result.inputs.changed_file);
    }
    emit_graph_footer(out);
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
