#pragma once

#include "hexagon/ports/driven/compile_database_port.h"

namespace xray::adapters::input {

class JsonDependencyProbe final : public xray::hexagon::ports::driven::CompileDatabasePort {
public:
    xray::hexagon::model::CompileDatabaseStatus load_compile_database() const override;
};

}  // namespace xray::adapters::input
