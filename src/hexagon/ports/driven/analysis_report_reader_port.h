#pragma once

#include <filesystem>
#include <string>

#include "hexagon/model/analysis_report_snapshot.h"

namespace xray::hexagon::ports::driven {

enum class AnalysisReportReadError {
    none,
    file_not_accessible,
    invalid_json,
    schema_mismatch,
    unsupported_report_type,
    incompatible_format_version,
    invalid_project_identity_source,
    unrecoverable_project_identity,
};

struct AnalysisReportReadResult {
    AnalysisReportReadError error{AnalysisReportReadError::none};
    std::string message;
    model::AnalysisReportSnapshot report;

    [[nodiscard]] bool is_success() const {
        return error == AnalysisReportReadError::none;
    }
};

class AnalysisReportReaderPort {
public:
    virtual ~AnalysisReportReaderPort() = default;

    virtual AnalysisReportReadResult read_analysis_report(
        const std::filesystem::path& path) const = 0;
};

}  // namespace xray::hexagon::ports::driven
