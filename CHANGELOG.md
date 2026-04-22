# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Docker `coverage` stage with `gcovr`-based line coverage reporting
- Docker `coverage-check` stage with configurable fail-under threshold during `docker build`
- CMake option `XRAY_ENABLE_COVERAGE` for instrumented project builds
- `compile_commands.json` parsing and validation via `CompileCommandsJsonAdapter`
- CLI with `analyze` subcommand and `--compile-commands` option using CLI11
- defined exit codes: `0` (success), `2` (CLI usage error), `3` (file not accessible), `4` (invalid input)
- diagnostic error messages with per-entry detail and collection cap
- `CompileEntry` and `CompileDatabaseResult` kernel models as immutable value objects
- adapter tests for all validation cases
- CLI integration tests and E2E binary tests against test data
- 9 test data directories under `tests/e2e/testdata/`

### Changed

- `CompileDatabasePort` now returns `CompileDatabaseResult` with entries and diagnostics
- `AnalyzeProjectPort::analyze_project` accepts a `compile_commands_path` parameter
- `ProjectAnalyzer` passes path through to driven port and evaluates result
- `main.cpp` composition root wired for M1 CLI path

### Removed

- `PlaceholderCliAdapter` and `JsonDependencyProbe` M0 placeholder adapters
- `CompileDatabaseStatus` replaced by `CompileDatabaseResult`

## [0.1.0] - 2026-04-21

### Added

- CMake-based project skeleton with `xray_hexagon`, `xray_adapters`, and `cmake-xray`
- placeholder M0 binary with hexagonal adapter and port wiring
- doctest-based placeholder test suite with `ctest` integration
- hexagon boundary check for forbidden adapter and third-party includes
- multi-stage `Dockerfile` with `build`, `test`, and `runtime` stages
- project documentation for architecture, roadmap, M0 plan, and releasing
- updated `README.md` with build, test, Docker, and usage examples
