#pragma once

#include <string>
#include <vector>

#include "diagnostic.h"
#include "translation_unit.h"

namespace xray::hexagon::model {

// AP M6-1.4 A.5 step 21pre: classification-side enums (origin, depth-kind)
// stay together with the IncludeHotspot they annotate. The filter-side
// enums (IncludeScope, IncludeDepthFilter) live in
// include_filter_options.h so request-side consumers (CLI adapter,
// port model) do not need this full hotspot header.

enum class IncludeOrigin {
    project,
    external,
    unknown,
};

enum class IncludeDepthKind {
    direct,
    indirect,
    mixed,
};

struct IncludeHotspot {
    std::string header_path;
    std::vector<TranslationUnitReference> affected_translation_units;
    std::vector<Diagnostic> diagnostics;
    IncludeOrigin origin{IncludeOrigin::unknown};
    IncludeDepthKind depth_kind{IncludeDepthKind::direct};
};

}  // namespace xray::hexagon::model
