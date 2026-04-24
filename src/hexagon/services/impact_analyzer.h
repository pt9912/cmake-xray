#pragma once

#include <filesystem>
#include <string_view>

#include "hexagon/ports/driven/build_model_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/ports/driving/analyze_impact_port.h"

namespace xray::hexagon::services {

class ImpactAnalyzer final : public ports::driving::AnalyzeImpactPort {
public:
    ImpactAnalyzer(const ports::driven::BuildModelPort& compile_db_port,
                   const ports::driven::IncludeResolverPort& include_resolver_port,
                   const ports::driven::BuildModelPort& file_api_port);

    model::ImpactResult analyze_impact(
        std::string_view compile_commands_path,
        const std::filesystem::path& changed_path,
        std::string_view cmake_file_api_path) const override;

private:
    const ports::driven::BuildModelPort& compile_db_port_;
    const ports::driven::BuildModelPort& file_api_port_;
    const ports::driven::IncludeResolverPort& include_resolver_port_;
};

}  // namespace xray::hexagon::services
