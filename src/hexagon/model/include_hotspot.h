#pragma once

#include <string>
#include <vector>

#include "diagnostic.h"
#include "translation_unit.h"

namespace xray::hexagon::model {

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

struct IncludeHotspot {
    std::string header_path;
    std::vector<TranslationUnitReference> affected_translation_units;
    std::vector<Diagnostic> diagnostics;
    IncludeOrigin origin{IncludeOrigin::unknown};
    IncludeDepthKind depth_kind{IncludeDepthKind::direct};
};

}  // namespace xray::hexagon::model
