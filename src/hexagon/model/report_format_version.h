#pragma once

namespace xray::hexagon::model {

// AP M6-1.4 A.3 flipped the constant from 3 to 4; A.4 shipped the
// structural include_filter block, hotspot.origin/depth_kind and the
// excluded_*_count fields across the JSON and DOT adapters and the
// schema. AP M6-1.5 A.3 raises the constant to 5: the v5 analyze JSON
// gains the analysis_configuration block, the analysis_section_states
// block, three new summary counters
// (tu_ranking_total_count_after_thresholds,
// tu_ranking_excluded_by_thresholds_count,
// include_hotspot_excluded_by_min_tus_count) and the new
// TargetGraphStatus::disabled value. The schema-side FormatVersion.const
// in spec/report-json.schema.json must stay synchronised with this
// constant; report_json_schema_validation CTest gate fails loud if the
// two constants drift apart.
inline constexpr int kReportFormatVersion = 5;

}  // namespace xray::hexagon::model
