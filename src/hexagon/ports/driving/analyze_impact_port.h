#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>

#include "hexagon/model/impact_result.h"
#include "hexagon/ports/driving/analyze_project_port.h"

namespace xray::hexagon::ports::driving {

struct AnalyzeImpactRequest {
    std::optional<InputPathArgument> compile_commands_path;
    InputPathArgument changed_file_path;
    std::optional<InputPathArgument> cmake_file_api_path;
    std::filesystem::path report_display_base;
    // AP M6-1.3 A.1: optional reverse-target-graph traversal depth in
    // [0, 32]. The CLI default of 2 is applied either by the CLI adapter
    // before the request is built, or, when the field is std::nullopt
    // (e.g. test or non-CLI adapter), by the ImpactAnalyzer service. A
    // value > 32 is a service-validation error
    // (impact_target_depth_out_of_range).
    std::optional<std::size_t> impact_target_depth;
    // AP M6-1.3 A.1: when true, a missing or unusable target graph
    // (target_graph_status=not_loaded) is rejected before result
    // construction with the service-validation error
    // target_graph_required. When false, missing target graph degrades
    // gracefully to an M5-compatible compile-DB-only result with a
    // dedicated diagnostic.
    bool require_target_graph{false};
};

class AnalyzeImpactPort {
public:
    virtual ~AnalyzeImpactPort() = default;

    virtual model::ImpactResult analyze_impact(AnalyzeImpactRequest request) const = 0;
};

}  // namespace xray::hexagon::ports::driving
