#pragma once

#include <string>
#include <vector>

#include "diagnostic.h"

namespace xray::hexagon::model {

struct ResolvedTranslationUnitIncludes {
    std::string translation_unit_key;
    std::vector<std::string> headers;
    std::vector<Diagnostic> diagnostics;
};

struct IncludeResolutionResult {
    bool heuristic{false};
    std::vector<ResolvedTranslationUnitIncludes> translation_units;
    std::vector<Diagnostic> diagnostics;
};

}  // namespace xray::hexagon::model
