#include "adapters/cli/placeholder_cli_adapter.h"

#include <CLI/CLI.hpp>

namespace xray::adapters::cli {

namespace {

CLI::App make_placeholder_app() {
    return CLI::App{"cmake-xray placeholder CLI adapter"};
}

}  // namespace

PlaceholderCliAdapter::PlaceholderCliAdapter(
    const xray::hexagon::ports::driving::AnalyzeProjectPort& analyze_project_port,
    const xray::hexagon::ports::driving::GenerateReportPort& generate_report_port)
    : analyze_project_port_(analyze_project_port),
      generate_report_port_(generate_report_port) {}

std::string PlaceholderCliAdapter::description() const {
    return make_placeholder_app().get_description();
}

std::string PlaceholderCliAdapter::run() const {
    [[maybe_unused]] const auto app = make_placeholder_app();
    const auto analysis_result = analyze_project_port_.analyze_project();
    return generate_report_port_.generate_report(analysis_result);
}

}  // namespace xray::adapters::cli
