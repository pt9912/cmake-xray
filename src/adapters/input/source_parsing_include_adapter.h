#pragma once

#include "hexagon/ports/driven/include_resolver_port.h"

namespace xray::adapters::input {

class SourceParsingIncludeAdapter final : public xray::hexagon::ports::driven::IncludeResolverPort {
public:
    xray::hexagon::model::IncludeResolutionResult resolve_includes(
        const std::vector<xray::hexagon::model::TranslationUnitObservation>& translation_units)
        const override;
};

}  // namespace xray::adapters::input
