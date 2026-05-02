#pragma once

namespace xray::hexagon::model {

// AP M6-1.4 A.5 step 21pre: filter-side enums extracted from the
// classification header so request-side consumers (CLI adapter, port
// model) do not pull in the IncludeHotspot struct just to type the
// scope/depth filter values.

enum class IncludeScope {
    all,
    project,
    external,
    unknown,
};

enum class IncludeDepthFilter {
    all,
    direct,
    indirect,
};

}  // namespace xray::hexagon::model
