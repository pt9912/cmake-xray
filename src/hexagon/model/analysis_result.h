#pragma once

#include "application_info.h"
#include "compile_database_result.h"
#include "diagnostic.h"
#include "include_hotspot.h"
#include "translation_unit.h"

namespace xray::hexagon::model {

struct AnalysisResult {
    ApplicationInfo application;
    CompileDatabaseResult compile_database;
    bool include_analysis_heuristic{false};
    std::vector<RankedTranslationUnit> translation_units;
    std::vector<IncludeHotspot> include_hotspots;
    std::vector<Diagnostic> diagnostics;
};

}  // namespace xray::hexagon::model
