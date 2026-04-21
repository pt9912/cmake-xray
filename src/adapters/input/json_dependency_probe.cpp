#include "adapters/input/json_dependency_probe.h"

#include <nlohmann/json.hpp>

namespace xray::adapters::input {

bool json_dependency_ready() {
    const auto probe = nlohmann::json::object();
    return probe.is_object();
}

}  // namespace xray::adapters::input
