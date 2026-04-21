#include "adapters/input/json_dependency_probe.h"

#include <nlohmann/json.hpp>

namespace xray::adapters::input {

hexagon::model::CompileDatabaseStatus JsonDependencyProbe::load_compile_database() const {
    const auto probe = nlohmann::json::object();
    return {
        .dependency_available = probe.is_object(),
    };
}

}  // namespace xray::adapters::input
