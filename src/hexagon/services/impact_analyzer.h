#pragma once

#include <filesystem>
#include <string_view>

#include "hexagon/ports/driven/compile_database_port.h"
#include "hexagon/ports/driven/include_resolver_port.h"
#include "hexagon/ports/driving/analyze_impact_port.h"

namespace xray::hexagon::services {

class ImpactAnalyzer final : public ports::driving::AnalyzeImpactPort {
public:
    ImpactAnalyzer(const ports::driven::CompileDatabasePort& compile_database_port,
                   const ports::driven::IncludeResolverPort& include_resolver_port);

    model::ImpactResult analyze_impact(std::string_view compile_commands_path,
                                       const std::filesystem::path& changed_path) const override;

private:
    const ports::driven::CompileDatabasePort& compile_database_port_;
    const ports::driven::IncludeResolverPort& include_resolver_port_;
};

}  // namespace xray::hexagon::services
