#pragma once

#include <map>

#include "analysis_configuration.h"
#include "application_info.h"
#include "compile_database_result.h"
#include "diagnostic.h"
#include "include_classification.h"
#include "include_filter_options.h"
#include "observation_source.h"
#include "report_inputs.h"
#include "target_graph.h"
#include "target_info.h"
#include "translation_unit.h"

namespace xray::hexagon::model {

struct AnalysisResult {
    ApplicationInfo application;
    CompileDatabaseResult compile_database;
    std::string compile_database_path;
    ReportInputs inputs;
    bool include_analysis_heuristic{false};
    std::vector<RankedTranslationUnit> translation_units;
    std::vector<IncludeHotspot> include_hotspots;
    std::vector<Diagnostic> diagnostics;
    ObservationSource observation_source{ObservationSource::exact};
    TargetMetadataStatus target_metadata{TargetMetadataStatus::not_loaded};
    std::vector<TargetAssignment> target_assignments;
    TargetGraph target_graph;
    TargetGraphStatus target_graph_status{TargetGraphStatus::not_loaded};
    std::vector<TargetInfo> target_hubs_in;
    std::vector<TargetInfo> target_hubs_out;
    IncludeScope include_scope_requested{IncludeScope::all};
    IncludeScope include_scope_effective{IncludeScope::all};
    IncludeDepthFilter include_depth_filter_requested{IncludeDepthFilter::all};
    IncludeDepthFilter include_depth_filter_effective{IncludeDepthFilter::all};
    std::size_t include_hotspot_total_count{0};
    std::size_t include_hotspot_returned_count{0};
    std::size_t include_hotspot_excluded_unknown_count{0};
    std::size_t include_hotspot_excluded_mixed_count{0};
    std::size_t include_depth_limit_requested{32};
    std::size_t include_depth_limit_effective{0};
    std::size_t include_node_budget_requested{10000};
    std::size_t include_node_budget_effective{0};
    bool include_node_budget_reached{false};
    AnalysisConfiguration analysis_configuration;
    std::map<AnalysisSection, AnalysisSectionState> analysis_section_states;
    // AP M6-1.5 A.2: threshold-filter statistics. Plan §488-498 introduces
    // three counters that adapters surface alongside the existing AP-1.4
    // include filter statistics:
    //  - tu_ranking_total_count_after_thresholds: TUs that survive
    //    --tu-threshold filtering, before --top limits.
    //  - tu_ranking_excluded_by_thresholds_count: TUs dropped by
    //    --tu-threshold filters.
    //  - include_hotspot_excluded_by_min_tus_count: hotspots dropped
    //    because affected_translation_units.size() < min_hotspot_tus.
    std::size_t tu_ranking_total_count_after_thresholds{0};
    std::size_t tu_ranking_excluded_by_thresholds_count{0};
    std::size_t include_hotspot_excluded_by_min_tus_count{0};
};

}  // namespace xray::hexagon::model
