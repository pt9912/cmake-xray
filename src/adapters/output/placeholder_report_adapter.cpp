#include "adapters/output/placeholder_report_adapter.h"

namespace xray::adapters::output {

std::string PlaceholderReportAdapter::write_report(
    const xray::hexagon::model::AnalysisResult& analysis_result) const {
    return std::string(analysis_result.application.name) + " " +
           std::string(analysis_result.application.version);
}

}  // namespace xray::adapters::output
