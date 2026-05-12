#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "hexagon/model/report_inputs.h"

namespace xray::adapters::input {

enum class AnalysisJsonReadError {
    none,
    file_not_accessible,
    invalid_json,
    schema_mismatch,
    unsupported_report_type,
    incompatible_format_version,
    invalid_project_identity_source,
    unrecoverable_project_identity,
};

struct AnalysisJsonReport {
    std::string path;
    int format_version{0};
    std::string project_identity;
    xray::hexagon::model::ProjectIdentitySource project_identity_source{
        xray::hexagon::model::ProjectIdentitySource::fallback_compile_database_fingerprint};
    nlohmann::json inputs;
    nlohmann::json summary;
    nlohmann::json analysis_configuration;
    nlohmann::json analysis_section_states;
    nlohmann::json include_filter;
    nlohmann::json translation_unit_ranking;
    nlohmann::json include_hotspots;
    nlohmann::json target_graph_status;
    nlohmann::json target_graph;
    nlohmann::json target_hubs;
    nlohmann::json diagnostics;
};

struct AnalysisJsonReadResult {
    AnalysisJsonReadError error{AnalysisJsonReadError::none};
    std::string error_message;
    AnalysisJsonReport report;

    [[nodiscard]] bool is_success() const {
        return error == AnalysisJsonReadError::none;
    }
};

class AnalysisJsonReader {
public:
    AnalysisJsonReadResult read(std::string_view path) const;
};

std::string project_identity_source_text(
    xray::hexagon::model::ProjectIdentitySource source);

}  // namespace xray::adapters::input
