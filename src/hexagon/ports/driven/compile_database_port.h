#pragma once

#include <string_view>

#include "hexagon/model/compile_database_result.h"

namespace xray::hexagon::ports::driven {

class CompileDatabasePort {
public:
    virtual ~CompileDatabasePort() = default;

    virtual model::CompileDatabaseResult load_compile_database(std::string_view path) const = 0;
};

}  // namespace xray::hexagon::ports::driven
