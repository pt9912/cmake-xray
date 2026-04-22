#include "services/report_generator.h"

namespace xray::hexagon::services {

ReportGenerator::ReportGenerator(const ports::driven::ReportWriterPort& report_writer_port)
    : report_writer_port_(report_writer_port) {}

std::string ReportGenerator::generate_analysis_report(const model::AnalysisResult& analysis_result,
                                                      std::size_t top_limit) const {
    return report_writer_port_.write_analysis_report(analysis_result, top_limit);
}

std::string ReportGenerator::generate_impact_report(
    const model::ImpactResult& impact_result) const {
    return report_writer_port_.write_impact_report(impact_result);
}

}  // namespace xray::hexagon::services
