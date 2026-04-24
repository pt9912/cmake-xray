#pragma once

#include <string_view>

#include "hexagon/ports/driven/build_model_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/ports/driving/analyze_project_port.h"

namespace xray::hexagon::services {

class ProjectAnalyzer final : public ports::driving::AnalyzeProjectPort {
public:
    ProjectAnalyzer(const ports::driven::BuildModelPort& compile_db_port,
                    const ports::driven::IncludeResolverPort& include_resolver_port,
                    const ports::driven::BuildModelPort& file_api_port);

    model::AnalysisResult analyze_project(
        ports::driving::AnalyzeProjectRequest request) const override;

private:
    const ports::driven::BuildModelPort& compile_db_port_;
    const ports::driven::BuildModelPort& file_api_port_;
    const ports::driven::IncludeResolverPort& include_resolver_port_;
};

}  // namespace xray::hexagon::services
