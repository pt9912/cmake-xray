#pragma once

#include "hexagon/ports/driven/compile_database_port.h"
#include "hexagon/ports/driving/analyze_project_port.h"

namespace xray::hexagon::services {

class ProjectAnalyzer final : public ports::driving::AnalyzeProjectPort {
public:
    explicit ProjectAnalyzer(const ports::driven::CompileDatabasePort& compile_database_port);

    model::AnalysisResult analyze_project() const override;

private:
    const ports::driven::CompileDatabasePort& compile_database_port_;
};

}  // namespace xray::hexagon::services
