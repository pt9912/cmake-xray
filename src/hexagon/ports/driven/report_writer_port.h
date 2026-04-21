#pragma once

#include <string>

#include "hexagon/model/analysis_result.h"

namespace xray::hexagon::ports::driven {

class ReportWriterPort {
public:
    virtual ~ReportWriterPort() = default;

    virtual std::string write_report(const model::AnalysisResult& analysis_result) const = 0;
};

}  // namespace xray::hexagon::ports::driven
