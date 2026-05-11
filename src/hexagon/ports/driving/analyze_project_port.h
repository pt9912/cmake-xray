#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <vector>

#include "hexagon/model/analysis_configuration.h"
#include "hexagon/model/analysis_result.h"
#include "hexagon/model/include_filter_options.h"

namespace xray::hexagon::ports::driving {

struct InputPathArgument {
    std::filesystem::path original_argument;
    std::filesystem::path path_for_io;
    bool was_relative{false};
};

struct AnalyzeProjectRequest {
    std::optional<InputPathArgument> compile_commands_path;
    std::optional<InputPathArgument> cmake_file_api_path;
    std::filesystem::path report_display_base;
    model::IncludeScope include_scope{model::IncludeScope::all};
    model::IncludeDepthFilter include_depth{model::IncludeDepthFilter::all};
    std::vector<model::AnalysisSection> analysis_sections;
    std::map<model::TuRankingMetric, std::size_t> tu_thresholds;
    std::size_t min_hotspot_tus{2};
    std::size_t target_hub_in_threshold{10};
    std::size_t target_hub_out_threshold{10};
};

class AnalyzeProjectPort {
public:
    virtual ~AnalyzeProjectPort() = default;

    virtual model::AnalysisResult analyze_project(AnalyzeProjectRequest request) const = 0;
};

}  // namespace xray::hexagon::ports::driving
