#pragma once

#include <string>
#include <vector>

#include "target_info.h"

namespace xray::hexagon::model {

enum class TargetDependencyKind {
    direct,
};

enum class TargetDependencyResolution {
    resolved,
    external,
};

struct TargetDependency {
    std::string from_unique_key;
    std::string from_display_name;
    std::string to_unique_key;
    std::string to_display_name;
    TargetDependencyKind kind{TargetDependencyKind::direct};
    TargetDependencyResolution resolution{TargetDependencyResolution::resolved};
};

enum class TargetGraphStatus {
    not_loaded,
    loaded,
    partial,
    // AP M6-1.5 A.3: disabled means "Target Graph section not requested via
    // --analysis". Semantically distinct from not_loaded ("requested, but
    // data not available"). The JSON serialization uses the
    // analysis_section_states block to distinguish; for the legacy
    // target_graph_status field, disabled lands as a new enum string.
    disabled,
};

struct TargetGraph {
    std::vector<TargetInfo> nodes;
    std::vector<TargetDependency> edges;
};

}  // namespace xray::hexagon::model
