#include "adapters/output/placeholder_report_adapter.h"

namespace xray::adapters::output {

std::string render_startup_message(std::string_view startup_message) {
    return std::string(startup_message);
}

}  // namespace xray::adapters::output
