#include "services/project_analyzer.h"

#include "model/application_info.h"

namespace xray::hexagon::services {

ProjectAnalyzer::ProjectAnalyzer(const ports::driven::CompileDatabasePort& compile_database_port)
    : compile_database_port_(compile_database_port) {}

model::AnalysisResult ProjectAnalyzer::analyze_project(
    std::string_view compile_commands_path) const {
    return {
        .application = model::application_info(),
        .compile_database = compile_database_port_.load_compile_database(compile_commands_path),
    };
}

}  // namespace xray::hexagon::services
