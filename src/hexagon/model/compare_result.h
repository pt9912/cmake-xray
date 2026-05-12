#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace xray::hexagon::model {

inline constexpr int kCompareFormatVersion = 1;

enum class CompareDiffKind {
    added,
    removed,
    changed,
};

struct CompareScalarValue;

using CompareScalarArray = std::vector<CompareScalarValue>;
using CompareScalarVariant =
    std::variant<std::monostate, std::string, std::int64_t, bool, CompareScalarArray>;

struct CompareScalarValue {
    CompareScalarVariant value;
};

struct CompareFieldChange {
    std::string field;
    CompareScalarValue baseline_value;
    CompareScalarValue current_value;
};

struct TranslationUnitDiff {
    CompareDiffKind kind{CompareDiffKind::changed};
    std::string source_path;
    std::string build_context_key;
    std::vector<CompareFieldChange> changes;
};

struct IncludeHotspotDiff {
    CompareDiffKind kind{CompareDiffKind::changed};
    std::string header_path;
    std::string include_origin;
    std::string include_depth_kind;
    std::vector<CompareFieldChange> changes;
};

struct TargetNodeDiff {
    CompareDiffKind kind{CompareDiffKind::changed};
    std::string unique_key;
    std::vector<CompareFieldChange> changes;
};

struct TargetEdgeDiff {
    CompareDiffKind kind{CompareDiffKind::changed};
    std::string from_unique_key;
    std::string to_unique_key;
    std::string kind_text;
    std::vector<CompareFieldChange> changes;
};

struct TargetHubDiff {
    CompareDiffKind kind{CompareDiffKind::changed};
    std::string unique_key;
    std::string direction;
};

struct CompareDiagnosticConfigurationDrift {
    std::string field;
    CompareScalarValue baseline_value;
    CompareScalarValue current_value;
    std::string severity{"warning"};
    std::string ci_policy_hint{"review_required"};
};

struct CompareDiagnosticDataAvailabilityDrift {
    std::string section;
    std::string baseline_state;
    std::string current_state;
};

struct CompareDiagnosticProjectIdentityDrift {
    std::string baseline_project_identity;
    std::string current_project_identity;
    std::size_t baseline_source_path_count{0};
    std::size_t current_source_path_count{0};
    std::size_t shared_source_path_count{0};
};

struct CompareInputs {
    std::string baseline_path;
    std::string current_path;
    int baseline_format_version{0};
    int current_format_version{0};
    std::optional<std::string> project_identity;
    std::string project_identity_source;
};

struct CompareSummary {
    int translation_units_added{0};
    int translation_units_removed{0};
    int translation_units_changed{0};
    int include_hotspots_added{0};
    int include_hotspots_removed{0};
    int include_hotspots_changed{0};
    int target_nodes_added{0};
    int target_nodes_removed{0};
    int target_nodes_changed{0};
    int target_edges_added{0};
    int target_edges_removed{0};
    int target_edges_changed{0};
    int target_hubs_added{0};
    int target_hubs_removed{0};
    int target_hubs_changed{0};
};

struct CompareResult {
    CompareInputs inputs;
    CompareSummary summary;
    std::vector<TranslationUnitDiff> translation_unit_diffs;
    std::vector<IncludeHotspotDiff> include_hotspot_diffs;
    std::vector<TargetNodeDiff> target_node_diffs;
    std::vector<TargetEdgeDiff> target_edge_diffs;
    std::vector<TargetHubDiff> target_hub_diffs;
    std::vector<CompareDiagnosticConfigurationDrift> configuration_drifts;
    std::vector<CompareDiagnosticDataAvailabilityDrift> data_availability_drifts;
    std::optional<CompareDiagnosticProjectIdentityDrift> project_identity_drift;
};

}  // namespace xray::hexagon::model
