# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- CMake File API as second primary input source via `--cmake-file-api <path>` for `analyze` and `impact` (`F-05`, `S-02`)
- `CmakeFileApiAdapter` for reading CMake File API v1 reply data with target extraction, compile group synthesis, and multi-target support
- `BuildModelPort` as unified driven port replacing the former `CompileDatabasePort` and `TargetMetadataPort` separation
- target-to-translation-unit assignment model with `TargetInfo`, `TargetAssignment`, and observation-key-based matching across adapters
- `ObservationSource` (`exact`/`derived`) and `TargetMetadataStatus` (`not_loaded`/`loaded`/`partial`) metadata on analysis and impact results
- target-aware `analyze` output: target suffixes `[targets: app, core]` on translation units and hotspots when file API data is loaded
- target-aware `impact` output: `directly affected targets` and `heuristically affected targets` sections with evidence-based classification (`F-19`, `F-24`)
- `analyze` and `impact` can run without `compile_commands.json` when file API data provides sufficient observations
- mixed-path support: when both `compile_commands.json` and file API are provided, the compile database remains the authoritative `exact` source; the file API enriches matching observations with target context
- diagnostics for partial target coverage, unmatched file API observations, and missing target metadata
- exit code `3` for inaccessible explicit `--cmake-file-api` paths and `4` for invalid or structurally insufficient reply data
- relative `--changed-file` paths in file-API-only mode resolve against the codemodel source root
- 26 test fixtures under `tests/e2e/testdata/m4/` covering valid, partial, multi-target, permuted, and error scenarios
- M4 golden output files for byte-exact E2E regression testing of file-api-only, multi-target, and mixed-path reports
- unit tests for relative path resolution via source root (file-API-only) and compile database directory (mixed path) with target attachment
- curated target-aware example outputs under `docs/examples/`

### Changed

- `AnalysisResult` and `ImpactResult` now carry `observation_source`, `target_metadata`, `target_assignments`, and `affected_targets` fields
- `RankedTranslationUnit` and `ImpactedTranslationUnit` now carry a `targets` vector for target-aware reporting
- console and markdown reporters show `observation source: exact|derived` and `target metadata: loaded|partial` when file API data is involved
- `impact` markdown report includes `## Directly Affected Targets` and `## Heuristically Affected Targets` sections before diagnostics
- application and project version bumped to `1.1.0`
- README updated with file API usage examples, target-aware impact documentation, and new example references
- release verification checklist extended with file API smoke tests

### Fixed

## [1.0.0] - 2026-04-22

### Added

- `MarkdownReportAdapter` with deterministic `analyze` and `impact` reports for human-readable artifacts and CI goldens
- CLI report selection via `--format console|markdown` and atomic markdown file output via `--output <path>`
- versioned M3 report fixtures, markdown golden files, example reports under `docs/examples/`, and E2E byte-compare coverage for console and markdown
- versioned reference projects under `tests/reference/scale_250`, `scale_500`, and `scale_1000` plus a checked-in generator for reproducible performance baselines
- `docs/performance.md` with measured baseline artifacts and explicit NF-04/NF-05 evaluation for the MVP path

### Changed

- report-wide diagnostics are now normalized in the kernel, sorted by `(severity, message)`, and shared consistently between console and markdown reporters
- `AnalysisResult` and `ImpactResult` now carry display-ready compile database paths for deterministic report context
- Docker toolchain images now include `time`, enabling reproducible in-container performance measurement paths
- application and project version bumped to `1.0.0`
- README and release documentation now describe the shipped MVP report paths, reference data, examples, and release checks

### Fixed

- console reporting no longer depends on reporter-side diagnostic deduplication and now matches the M3 report contract
- `impact` markdown classification and report-wide diagnostics remain stable for permuted compile database orderings
- markdown output writes leave no partial target files under the destination name and surface report write failures via exit code `1`

## [0.3.0] - 2026-04-22

### Added

- `analyze --top <n>` with deterministic translation-unit ranking based on `arg_count`, `include_path_count`, and `define_count`
- heuristically marked include-hotspot analysis with full translation-unit assignment per hotspot
- `impact --compile-commands <path> --changed-file <path>` for direct and include-derived translation-unit impact analysis
- `SourceParsingIncludeAdapter` with recursive include traversal, search-path handling, cycle protection, and unresolved-include diagnostics
- `ConsoleReportAdapter` for structured `analyze` and `impact` console output
- M2 adapter, hexagon, CLI, and binary E2E coverage for ranking, hotspots, impact, path semantics, and heuristic messaging
- GitHub Actions workflows for Docker-based build and test validation
- CI artifact upload for coverage reports (`summary.txt` and `coverage.txt`)
- Docker `quality` stage with `clang-tidy` and `lizard` reports
- Docker `quality-check` stage for static-analysis and metrics gates
- repository-wide `.clang-tidy` baseline configuration

### Changed

- `CompileEntry` now preserves whether compile data originated from `arguments` or raw `command`
- `ProjectAnalyzer` now returns ranked translation units, include hotspots, and inline diagnostics in `AnalysisResult`
- `AnalyzeImpactPort`, `ReportWriterPort`, and `GenerateReportPort` now operate on typed M2 result models instead of placeholder strings
- application and project version bumped to `0.3.0`
- `coverage-check` now enforces a default minimum line coverage threshold of `100%`
- coverage and release documentation now use the Docker-based coverage gate consistently
- CI now publishes quality artifacts alongside coverage artifacts

### Fixed

- `analyze` preserves TU-local diagnostics even when `--top` hides lower-ranked translation units
- `impact` now surfaces concrete include-resolution diagnostics alongside heuristic results
- CLI exit-code mapping now handles unexpected compile database errors consistently
- CLI and E2E tests now cover additional help, diagnostic truncation, and unexpected-error paths, raising line coverage under `src/` to `100%`

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
