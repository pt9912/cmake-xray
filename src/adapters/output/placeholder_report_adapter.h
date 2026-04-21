#pragma once

#include <string>
#include <string_view>

namespace xray::adapters::output {

std::string render_startup_message(std::string_view startup_message);

}  // namespace xray::adapters::output
