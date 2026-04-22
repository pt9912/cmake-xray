#pragma once

#include <string_view>

#include "hexagon/ports/driven/compile_database_port.h"

namespace xray::adapters::input {

class CompileCommandsJsonAdapter final
    : public xray::hexagon::ports::driven::CompileDatabasePort {
public:
    xray::hexagon::model::CompileDatabaseResult load_compile_database(
        std::string_view path) const override;
};

}  // namespace xray::adapters::input
