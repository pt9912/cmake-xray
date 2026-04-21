#pragma once

#include "hexagon/model/compile_database_status.h"

namespace xray::hexagon::ports::driven {

class CompileDatabasePort {
public:
    virtual ~CompileDatabasePort() = default;

    virtual model::CompileDatabaseStatus load_compile_database() const = 0;
};

}  // namespace xray::hexagon::ports::driven
