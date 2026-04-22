#pragma once

#include <string>
#include <vector>

#include "application_info.h"
#include "compile_database_result.h"
#include "diagnostic.h"
#include "translation_unit.h"

namespace xray::hexagon::model {

enum class ImpactKind {
    direct,
    heuristic,
};

struct ImpactedTranslationUnit {
    TranslationUnitReference reference;
    ImpactKind kind{ImpactKind::direct};
};

struct ImpactResult {
    ApplicationInfo application;
    CompileDatabaseResult compile_database;
    std::string compile_database_path;
    std::string changed_file;
    std::string changed_file_key;
    bool heuristic{false};
    std::vector<ImpactedTranslationUnit> affected_translation_units;
    std::vector<Diagnostic> diagnostics;
};

}  // namespace xray::hexagon::model
