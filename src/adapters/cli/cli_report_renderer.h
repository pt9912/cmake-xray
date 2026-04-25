#pragma once

#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace xray::adapters::cli {

struct RenderError {
    std::string message;
};

struct RenderResult {
    std::optional<std::string> content;
    std::optional<RenderError> error;
};

class CliReportRenderer {
public:
    virtual ~CliReportRenderer() = default;
    virtual RenderResult render() const = 0;
};

// Lifts an existing string-returning report adapter call into the RenderResult
// channel. Caught exceptions are mapped to RenderError as a safety net so the
// CLI write path can refuse to touch the target file. Tests should still
// exercise the explicit RenderResult-with-error path through a dedicated
// CliReportRenderer doppelgaenger; the catch is not the primary error contract.
class StringFunctionCliReportRenderer final : public CliReportRenderer {
public:
    using RenderFn = std::function<std::string()>;

    explicit StringFunctionCliReportRenderer(RenderFn fn) : fn_(std::move(fn)) {}

    RenderResult render() const override {
        try {
            return RenderResult{fn_(), std::nullopt};
        } catch (const std::exception& e) {
            return RenderResult{std::nullopt, RenderError{e.what()}};
        }
    }

private:
    RenderFn fn_;
};

}  // namespace xray::adapters::cli
