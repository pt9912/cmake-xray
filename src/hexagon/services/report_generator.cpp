#include "services/report_generator.h"

namespace xray::hexagon::services {

ReportGenerator::ReportGenerator(const ports::driven::ReportWriterPort& report_writer_port)
    : report_writer_port_(report_writer_port) {}

std::string ReportGenerator::generate_report(const model::AnalysisResult& analysis_result) const {
    return report_writer_port_.write_report(analysis_result);
}

}  // namespace xray::hexagon::services
