#pragma once

#include "application_info.h"
#include "compile_database_result.h"
#include "diagnostic.h"
#include "include_hotspot.h"
#include "observation_source.h"
#include "report_inputs.h"
#include "target_info.h"
#include "translation_unit.h"

namespace xray::hexagon::model {

struct AnalysisResult {
    ApplicationInfo application;
    CompileDatabaseResult compile_database;
    std::string compile_database_path;
    ReportInputs inputs;
    bool include_analysis_heuristic{false};
    std::vector<RankedTranslationUnit> translation_units;
    std::vector<IncludeHotspot> include_hotspots;
    std::vector<Diagnostic> diagnostics;
    ObservationSource observation_source{ObservationSource::exact};
    TargetMetadataStatus target_metadata{TargetMetadataStatus::not_loaded};
    std::vector<TargetAssignment> target_assignments;
};

}  // namespace xray::hexagon::model
