#pragma once

#include <string_view>

#include "hexagon/ports/driven/compile_database_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/ports/driving/analyze_project_port.h"

namespace xray::hexagon::services {

class ProjectAnalyzer final : public ports::driving::AnalyzeProjectPort {
public:
    ProjectAnalyzer(const ports::driven::CompileDatabasePort& compile_database_port,
                    const ports::driven::IncludeResolverPort& include_resolver_port);

    model::AnalysisResult analyze_project(std::string_view compile_commands_path) const override;

private:
    const ports::driven::CompileDatabasePort& compile_database_port_;
    const ports::driven::IncludeResolverPort& include_resolver_port_;
};

}  // namespace xray::hexagon::services
