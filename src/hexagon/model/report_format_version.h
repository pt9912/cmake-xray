#pragma once

namespace xray::hexagon::model {

// AP M6-1.4 A.3 flipped the constant from 3 to 4; A.4 shipped the
// structural include_filter block, hotspot.origin/depth_kind and the
// excluded_*_count fields across the JSON and DOT adapters and the
// schema. The schema-side FormatVersion.const in
// spec/report-json.schema.json must stay synchronised with this
// constant; report_json_schema_validation CTest gate fails loud if the
// two constants drift apart.
inline constexpr int kReportFormatVersion = 4;

}  // namespace xray::hexagon::model
