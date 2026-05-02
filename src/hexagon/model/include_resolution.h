#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "diagnostic.h"
#include "include_hotspot.h"

namespace xray::hexagon::model {

struct IncludeEntry {
    std::string header_path;
    IncludeDepthKind depth_kind{IncludeDepthKind::direct};
};

struct ResolvedTranslationUnitIncludes {
    std::string translation_unit_key;
    std::vector<IncludeEntry> headers;
    std::vector<Diagnostic> diagnostics;
};

struct IncludeResolutionResult {
    bool heuristic{false};
    std::vector<ResolvedTranslationUnitIncludes> translation_units;
    std::vector<Diagnostic> diagnostics;
    std::size_t include_depth_limit_requested{32};
    std::size_t include_depth_limit_effective{0};
    std::size_t include_node_budget_requested{10000};
    std::size_t include_node_budget_effective{0};
    bool include_node_budget_reached{false};
};

}  // namespace xray::hexagon::model
