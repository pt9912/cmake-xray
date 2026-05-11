#pragma once

#include <cstddef>
#include <map>
#include <vector>

namespace xray::hexagon::model {

// AP M6-1.5 A.1: enums and configuration struct for the configurable
// analysis surface (sections, TU-ranking thresholds, hub thresholds,
// hotspot min-TU). Lives in its own header so request/result types and
// adapters that only need the enums do not pull in the full
// AnalysisResult / IncludeHotspot tree. The header is schema-free in
// A.1 — A.3 raises kReportFormatVersion to 5 and migrates schema.

enum class AnalysisSection {
    tu_ranking,
    include_hotspots,
    target_graph,
    target_hubs,
};

enum class AnalysisSectionState {
    active,
    disabled,
    not_loaded,
};

enum class TuRankingMetric {
    arg_count,
    include_path_count,
    define_count,
};

// AnalysisConfiguration carries the resolved (requested + effective)
// section selection and the four numeric thresholds. Defaults mirror the
// pre-AP-1.5 hardcoded behaviour: hub thresholds 10/10, hotspot min-TU
// 2, no per-metric TU thresholds.
struct AnalysisConfiguration {
    std::vector<AnalysisSection> requested_sections;
    std::vector<AnalysisSection> effective_sections;
    std::map<TuRankingMetric, std::size_t> tu_thresholds;
    std::size_t min_hotspot_tus{2};
    std::size_t target_hub_in_threshold{10};
    std::size_t target_hub_out_threshold{10};
};

}  // namespace xray::hexagon::model
