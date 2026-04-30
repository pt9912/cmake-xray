#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "compile_database_result.h"
#include "diagnostic.h"
#include "observation_source.h"
#include "target_graph.h"
#include "target_info.h"

namespace xray::hexagon::model {

struct BuildModelResult {
    ObservationSource source{ObservationSource::exact};
    TargetMetadataStatus target_metadata{TargetMetadataStatus::not_loaded};
    CompileDatabaseResult compile_database;
    std::vector<TargetAssignment> target_assignments;
    std::string source_root;
    std::vector<Diagnostic> diagnostics;
    std::optional<std::filesystem::path> cmake_file_api_resolved_path;
    TargetGraph target_graph;
    TargetGraphStatus target_graph_status{TargetGraphStatus::not_loaded};

    bool is_success() const { return compile_database.is_success(); }
};

}  // namespace xray::hexagon::model
