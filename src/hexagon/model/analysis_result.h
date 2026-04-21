#pragma once

#include <string_view>

#include "application_info.h"
#include "compile_database_status.h"

namespace xray::hexagon::model {

struct AnalysisResult {
    ApplicationInfo application;
    CompileDatabaseStatus compile_database;
    std::string_view summary;
};

}  // namespace xray::hexagon::model
