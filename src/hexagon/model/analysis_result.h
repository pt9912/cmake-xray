#pragma once

#include "application_info.h"
#include "compile_database_result.h"

namespace xray::hexagon::model {

struct AnalysisResult {
    ApplicationInfo application;
    CompileDatabaseResult compile_database;
};

}  // namespace xray::hexagon::model
