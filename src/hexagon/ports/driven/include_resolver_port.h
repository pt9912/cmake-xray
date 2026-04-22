#pragma once

#include <vector>

#include "hexagon/model/include_resolution.h"
#include "hexagon/model/translation_unit.h"

namespace xray::hexagon::ports::driven {

class IncludeResolverPort {
public:
    virtual ~IncludeResolverPort() = default;

    virtual model::IncludeResolutionResult resolve_includes(
        const std::vector<model::TranslationUnitObservation>& translation_units) const = 0;
};

}  // namespace xray::hexagon::ports::driven
