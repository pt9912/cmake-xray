#pragma once

#include <string>
#include <vector>

namespace xray::hexagon::model {

struct CompileEntry {
    std::string file;
    std::string directory;
    std::vector<std::string> arguments;
};

}  // namespace xray::hexagon::model
