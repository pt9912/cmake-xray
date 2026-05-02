#pragma once

namespace xray::hexagon::model {

// AP M6-1.3 A.4 activates v3 across all five report formats. The
// schema-side FormatVersion.const in spec/report-json.schema.json
// must stay synchronised with this constant; report_json_schema_validation
// CTest gate fails loud if they drift apart.
inline constexpr int kReportFormatVersion = 3;

}  // namespace xray::hexagon::model
