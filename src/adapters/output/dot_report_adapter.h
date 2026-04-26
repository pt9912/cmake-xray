#pragma once

#include <cstddef>

#include "hexagon/ports/driven/report_writer_port.h"

namespace xray::adapters::output {

// Graph budgets per docs/report-dot.md "Budgets" sections. Pure values,
// computed from the effective --top limit (analyze) or fixed M5 constants
// (impact). Exposed publicly so unit tests can pin the formulas without
// needing to instantiate the adapter or parse the rendered DOT.
struct DotBudget {
    std::size_t node_limit;
    std::size_t edge_limit;
    // 0 for impact, since impact has no per-hotspot context concept.
    std::size_t context_limit;
};

DotBudget compute_analyze_budget(std::size_t top_limit);
DotBudget compute_impact_budget();

class DotReportAdapter final : public xray::hexagon::ports::driven::ReportWriterPort {
public:
    std::string write_analysis_report(
        const xray::hexagon::model::AnalysisResult& analysis_result,
        std::size_t top_limit) const override;
    std::string write_impact_report(
        const xray::hexagon::model::ImpactResult& impact_result) const override;
};

}  // namespace xray::adapters::output
