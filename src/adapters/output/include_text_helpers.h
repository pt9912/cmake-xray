#pragma once

#include <string>

#include "hexagon/model/include_classification.h"
#include "hexagon/model/include_filter_options.h"

namespace xray::adapters::output {

// AP M6-1.4 A.5 step-21 review-fix item 4: shared text helpers for the
// four include-related enums. Originally duplicated across the JSON,
// DOT and HTML adapters; centralised here before the Markdown and
// Console adapters in A.5 step 22/23 grow their own copies. The
// returned string values are part of the JSON / DOT / HTML / Markdown /
// Console contracts pinned in plan-M6-1-4.md (Reportausgabe), so
// changes here ripple into the goldens of every report format.

inline std::string include_origin_text(xray::hexagon::model::IncludeOrigin origin) {
    if (origin == xray::hexagon::model::IncludeOrigin::project) return "project";
    if (origin == xray::hexagon::model::IncludeOrigin::external) return "external";
    return "unknown";
}

inline std::string include_depth_kind_text(xray::hexagon::model::IncludeDepthKind kind) {
    if (kind == xray::hexagon::model::IncludeDepthKind::direct) return "direct";
    if (kind == xray::hexagon::model::IncludeDepthKind::indirect) return "indirect";
    return "mixed";
}

inline std::string include_scope_text(xray::hexagon::model::IncludeScope scope) {
    if (scope == xray::hexagon::model::IncludeScope::project) return "project";
    if (scope == xray::hexagon::model::IncludeScope::external) return "external";
    if (scope == xray::hexagon::model::IncludeScope::unknown) return "unknown";
    return "all";
}

inline std::string include_depth_filter_text(xray::hexagon::model::IncludeDepthFilter filter) {
    if (filter == xray::hexagon::model::IncludeDepthFilter::direct) return "direct";
    if (filter == xray::hexagon::model::IncludeDepthFilter::indirect) return "indirect";
    return "all";
}

}  // namespace xray::adapters::output
