#pragma once

#include <string_view>

#include "hexagon/model/build_model_result.h"

namespace xray::hexagon::ports::driven {

class BuildModelPort {
public:
    virtual ~BuildModelPort() = default;

    virtual model::BuildModelResult load_build_model(std::string_view path) const = 0;
};

}  // namespace xray::hexagon::ports::driven
