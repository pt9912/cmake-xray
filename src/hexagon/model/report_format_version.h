#pragma once

namespace xray::hexagon::model {

// AP M6-1.4 A.3 bumps v4 alongside the new --include-scope/--include-depth
// CLI options and the schema FormatVersion.const flip in
// spec/report-json.schema.json. The structural include_filter / origin /
// depth_kind additions land in AP M6-1.4 A.4 with the JSON adapter
// rollout; A.3 only flips the version constant and migrates the existing
// goldens. report_json_schema_validation CTest gate fails loud if the
// two constants drift apart.
inline constexpr int kReportFormatVersion = 4;

}  // namespace xray::hexagon::model
