#pragma once

#include <string_view>

#include "hexagon/ports/driven/build_model_port.h"

namespace xray::adapters::input {

class CmakeFileApiAdapter final : public xray::hexagon::ports::driven::BuildModelPort {
public:
    xray::hexagon::model::BuildModelResult load_build_model(
        std::string_view path) const override;
};

}  // namespace xray::adapters::input
