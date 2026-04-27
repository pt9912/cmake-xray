#pragma once

namespace xray::adapters::cli {

// AP M5-1.5: CLI-internal verbosity enum. The value lives in CliOptions
// (cli_adapter.cpp's anonymous namespace) and never crosses into hexagon
// services, driving ports, driven ports or output adapters; verbosity is a
// CLI emission policy, not a report format parameter. tests/adapters/
// test_port_wiring.cpp pins the architectural boundary by constructing
// every output adapter without a verbosity argument.
enum class OutputVerbosity {
    normal,
    quiet,
    verbose,
};

}  // namespace xray::adapters::cli
