#pragma once

#include <string>
#include <vector>

#include "diagnostic.h"
#include "translation_unit.h"

namespace xray::hexagon::model {

struct IncludeHotspot {
    std::string header_path;
    std::vector<TranslationUnitReference> affected_translation_units;
    std::vector<Diagnostic> diagnostics;
};

}  // namespace xray::hexagon::model
