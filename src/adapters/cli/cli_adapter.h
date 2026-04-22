#pragma once

#include <ostream>

#include "hexagon/ports/driving/analyze_project_port.h"

namespace xray::adapters::cli {

class CliAdapter {
public:
    explicit CliAdapter(
        const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port);

    int run(int argc, const char* const* argv, std::ostream& out, std::ostream& err) const;

private:
    const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port_;
};

}  // namespace xray::adapters::cli
