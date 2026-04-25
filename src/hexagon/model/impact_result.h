#pragma once

#include <string>
#include <vector>

#include "application_info.h"
#include "compile_database_result.h"
#include "diagnostic.h"
#include "observation_source.h"
#include "report_inputs.h"
#include "target_info.h"
#include "translation_unit.h"

namespace xray::hexagon::model {

enum class ImpactKind {
    direct,
    heuristic,
};

struct ImpactedTranslationUnit {
    TranslationUnitReference reference;
    ImpactKind kind{ImpactKind::direct};
    std::vector<TargetInfo> targets;
};

enum class TargetImpactClassification {
    direct,
    heuristic,
};

struct ImpactedTarget {
    TargetInfo target;
    TargetImpactClassification classification{TargetImpactClassification::direct};
};

struct ImpactResult {
    ApplicationInfo application;
    CompileDatabaseResult compile_database;
    std::string compile_database_path;
    std::string changed_file;
    std::string changed_file_key;
    ReportInputs inputs;
    bool heuristic{false};
    std::vector<ImpactedTranslationUnit> affected_translation_units;
    std::vector<Diagnostic> diagnostics;
    ObservationSource observation_source{ObservationSource::exact};
    TargetMetadataStatus target_metadata{TargetMetadataStatus::not_loaded};
    std::vector<TargetAssignment> target_assignments;
    std::vector<ImpactedTarget> affected_targets;
};

}  // namespace xray::hexagon::model
