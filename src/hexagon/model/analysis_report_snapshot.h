#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "report_inputs.h"

namespace xray::hexagon::model {

struct CompareDiagnosticSnapshot {
    std::string severity;
    std::string message;
};

struct CompareTargetSnapshot {
    std::string display_name;
    std::string type;
    std::string unique_key;
};

struct CompareTranslationUnitSnapshot {
    std::string source_path;
    std::string build_context_key;
    int arg_count{0};
    int include_path_count{0};
    int define_count{0};
    int score{0};
    std::vector<CompareTargetSnapshot> targets;
    std::vector<CompareDiagnosticSnapshot> diagnostics;
};

struct CompareHotspotAffectedUnitSnapshot {
    std::string source_path;
    std::string build_context_key;
};

struct CompareIncludeHotspotSnapshot {
    std::string header_path;
    std::string include_origin;
    std::string include_depth_kind;
    int affected_total_count{0};
    std::vector<CompareHotspotAffectedUnitSnapshot> affected_translation_units;
    std::vector<CompareDiagnosticSnapshot> diagnostics;
};

struct CompareTargetEdgeSnapshot {
    std::string from_unique_key;
    std::string to_unique_key;
    std::string kind;
    std::string resolution;
    std::string from_display_name;
    std::string to_display_name;
};

struct CompareConfigurationSnapshot {
    std::vector<std::string> analysis_sections;
    int tu_threshold_arg_count{0};
    int tu_threshold_include_path_count{0};
    int tu_threshold_define_count{0};
    int min_hotspot_tus{0};
    int target_hub_in_threshold{0};
    int target_hub_out_threshold{0};
    std::string include_scope;
    std::string include_depth;
};

struct AnalysisReportSnapshot {
    std::string path;
    int format_version{0};
    std::string project_identity;
    ProjectIdentitySource project_identity_source{
        ProjectIdentitySource::fallback_compile_database_fingerprint};
    CompareConfigurationSnapshot configuration;
    std::string target_graph_state{"not_loaded"};
    std::string target_hubs_state{"not_loaded"};
    std::vector<CompareTranslationUnitSnapshot> translation_units;
    std::vector<CompareIncludeHotspotSnapshot> include_hotspots;
    std::vector<CompareTargetSnapshot> target_nodes;
    std::vector<CompareTargetEdgeSnapshot> target_edges;
    std::vector<CompareTargetSnapshot> target_hubs_in;
    std::vector<CompareTargetSnapshot> target_hubs_out;
};

}  // namespace xray::hexagon::model
