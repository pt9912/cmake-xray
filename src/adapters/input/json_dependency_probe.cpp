#include "adapters/input/json_dependency_probe.h"

#include <nlohmann/json.hpp>

namespace xray::adapters::input {

hexagon::model::CompileDatabaseResult JsonDependencyProbe::load_compile_database(
    std::string_view /*path*/) const {
    const auto probe = nlohmann::json::object();
    static_cast<void>(probe);
    return {};
}

}  // namespace xray::adapters::input
