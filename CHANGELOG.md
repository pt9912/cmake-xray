# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- GitHub Actions workflows for Docker-based build and test validation
- CI artifact upload for coverage reports (`summary.txt` and `coverage.txt`)
- Docker `quality` stage with `clang-tidy` and `lizard` reports
- Docker `quality-check` stage for static-analysis and metrics gates
- repository-wide `.clang-tidy` baseline configuration

### Changed

- `coverage-check` now enforces a default minimum line coverage threshold of `100%`
- coverage and release documentation now use the Docker-based coverage gate consistently
- CI now publishes quality artifacts alongside coverage artifacts

### Fixed

- CLI exit-code mapping now handles unexpected compile database errors consistently
- CLI and E2E tests now cover additional help, diagnostic truncation, and unexpected-error paths, raising line coverage under `src/` to `100%`

## [0.3.0] - 2026-04-22

### Added

- `analyze --top <n>` with deterministic translation-unit ranking based on `arg_count`, `include_path_count`, and `define_count`
- heuristically marked include-hotspot analysis with full translation-unit assignment per hotspot
- `impact --compile-commands <path> --changed-file <path>` for direct and include-derived translation-unit impact analysis
- `SourceParsingIncludeAdapter` with recursive include traversal, search-path handling, cycle protection, and unresolved-include diagnostics
- `ConsoleReportAdapter` for structured `analyze` and `impact` console output
- M2 adapter, hexagon, CLI, and binary E2E coverage for ranking, hotspots, impact, path semantics, and heuristic messaging

### Changed

- `CompileEntry` now preserves whether compile data originated from `arguments` or raw `command`
- `ProjectAnalyzer` now returns ranked translation units, include hotspots, and inline diagnostics in `AnalysisResult`
- `AnalyzeImpactPort`, `ReportWriterPort`, and `GenerateReportPort` now operate on typed M2 result models instead of placeholder strings
- application and project version bumped to `0.3.0`

## [0.2.0] - 2026-04-22

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
