#pragma once

namespace xray::adapters::cli {

struct ExitCode {
    static constexpr int success = 0;
    static constexpr int unexpected_error = 1;
    static constexpr int cli_usage_error = 2;
    static constexpr int input_not_accessible = 3;
    static constexpr int input_invalid = 4;
};

}  // namespace xray::adapters::cli
