#pragma once

#include <string>

#include "hexagon/model/compare_result.h"

namespace xray::adapters::output {

class MarkdownCompareAdapter final {
public:
    std::string write_compare_report(
        const xray::hexagon::model::CompareResult& result) const;
};

}  // namespace xray::adapters::output
