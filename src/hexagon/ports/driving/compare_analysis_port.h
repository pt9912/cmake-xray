#pragma once

#include <filesystem>

#include "hexagon/model/compare_result.h"

namespace xray::hexagon::ports::driving {

struct CompareAnalysisRequest {
    std::filesystem::path baseline_path;
    std::filesystem::path current_path;
    bool allow_project_identity_drift{false};
};

class CompareAnalysisPort {
public:
    virtual ~CompareAnalysisPort() = default;

    virtual model::CompareResult compare(CompareAnalysisRequest request) const = 0;
};

}  // namespace xray::hexagon::ports::driving
