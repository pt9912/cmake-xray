# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- None.

## [1.3.0] - 2026-05-13

M6 adds the target graph, impact prioritisation, include classification,
analysis filtering and report comparison surface. This release is a
breaking report-format update: regenerate analyze/impact JSON reports
before using v1.3.0 tools or the new `compare` subcommand.

### Added

- Target graph extraction from CMake File API replies, including cycle
  handling, external dependency markers and id-collision diagnostics.
- Target graph and target hub sections in console, markdown, HTML, JSON
  and DOT report formats.
- Impact prioritisation via reverse target-graph BFS, with new CLI
  options `--impact-target-depth` and `--require-target-graph`.
- Include hotspot classification by origin (`project`, `external`,
  `unknown`) and depth (`direct`, `indirect`, `mixed`), plus analyze
  filters `--include-scope` and `--include-depth`.
- Configurable analysis selection and thresholds via `--analysis`,
  `--tu-threshold`, `--min-hotspot-tus`,
  `--target-hub-in-threshold` and `--target-hub-out-threshold`.
- Project identity in analyze JSON, derived from the CMake File API
  source root or a deterministic compile-database fingerprint.
- New `cmake-xray compare` subcommand for diffing two analyze JSON
  reports, with console, markdown and JSON output plus structured drift
  diagnostics.

### Changed

- Project and application version bumped to `1.3.0`.
- Analyze/impact `kReportFormatVersion` rises from `1` to `6`; compare
  reports start at `kCompareFormatVersion == 1`.
- DOT graph attributes now include target graph status, include filter
  values, include budget state, analysis configuration and impact
  target-depth metadata.
- HTML report sections now include analysis configuration, target
  graph, target hubs and prioritised affected targets where applicable.
- HTML evidence badges rename from single-dash to BEM double-dash to
  align with the new origin/depth classes: `badge-direct` →
  `badge--evidence-direct`, `badge-heuristic` →
  `badge--evidence-heuristic`. The visible badge text stays
  unchanged.
- README.md and docs/user/guide.md now explicitly document the
  `cmake -B build`-aus-Source path as the supported user-facing
  installation path on macOS arm64 and Windows x86_64 (where no
  release artefact is shipped), with platform-specific output paths
  (`./build/cmake-xray` on Linux/macOS, `./build/Release/cmake-xray.exe`
  on Windows). The previous "fuer Entwicklung" label suggested the
  source build was developer-only; on the two `validated_smoke`
  platforms it is the supported user path.
- macOS arm64 and Windows x86_64 platform status promoted from
  `known_limited` to `validated_smoke` in `README.md`,
  `docs/user/guide.md`, `docs/user/releasing.md`, `docs/user/quality.md` and
  `docs/planning/plan-M5-1-7.md`. Both external preconditions from
  `docs/planning/plan-M5-1-7.md` "Plattformstatus-Vertrag" are now met:
  Branch-Protection on `main` anchors the five Required Checks
  (`Native (linux-x86_64)`, `Native (macos-arm64)`,
  `Native (windows-x86_64)`, `Docker Runtime Build`, `docs/examples
  Host-Portability`), and build.yml run 25153798452 (commit
  `7be4829`, post-protection) is the auditable green CI run on the
  platform runners. The `[1.2.0]` platform-status table stays
  unchanged because it is the release-time record; the upgrade
  ships in the next release.

### Fixed

- Compare CI gate gaps from AP 1.6: reader error paths, bool/null scalar
  rendering and CLI compare validation now have coverage sufficient for
  the 100% Docker coverage gate, and compare-related functions are below
  the Lizard warning thresholds.
- `scripts/oci-image-publish.sh` `latest` command no longer aborts
  when `:latest` already points at a different digest than the new
  versioned tag. The pre-push abort blocked every release after the
  first regular release, because `:latest` legitimately carries the
  previous stable release's digest at that moment. The post-push
  digest-verify on `:latest` is unchanged and remains the correctness
  guarantee for the normal flow; Fall 4 of `docs/user/releasing.md`
  Recovery-Runbook stays the manual path for stuck-`latest`
  inspections.
- `read_remote_digest` in `scripts/oci-image-publish.sh` now extracts
  the digest robustly across buildx versions. buildx 0.30.x (Ubuntu
  24.04's docker package, the GHA ubuntu-latest runner) silently
  ignores `--format '{{.Manifest.Digest}}'` and prints the full
  `Name/MediaType/Digest` block; buildx >= 0.33 honours the template
  and prints just the digest string. The post-push digest comparison
  in `cmd_latest` previously compared the multi-line block as-is,
  which differed between `:latest` and `:X.Y.Z` only in the `Name:`
  field, so the verify always failed once it was reached. The awk
  filter accepts both buildx output shapes.
- Regression coverage for both fixes: new stable-version scenario in
  `tests/release/test_release_dry_run.sh` exercises the `cmd_latest`
  path end-to-end against a pre-seeded `:latest` digest (the
  prerelease versions in scenarios 1-9 all skip `cmd_latest`, which
  is why both bugs shipped uncovered).

### Migration

- pre-M6 JSON reports are not consumable by v1.3.0 compare flows. Re-run
  cmake-xray on your project to regenerate reports with
  `format_version: 6` before using `cmake-xray compare`.
- `cmake-xray compare` accepts only analyze JSON reports. Impact compare
  remains post-M6.
- JSON consumers must account for the closed-schema additions in M6:
  target graph fields, include filter fields, analysis configuration and
  project identity are part of the v6 contract.

### Platform status

| Platform | Status |
|---|---|
| Linux x86_64 | `supported` |
| macOS arm64 | `validated_smoke` |
| Windows x86_64 | `validated_smoke` |

## [1.2.0] - 2026-04-30

M5 hardens the report formats, ships a reproducible release pipeline and
documents the platform support boundary. No breaking changes for users on
the v1.1.0 contract: every new flag and report format is opt-in, the JSON
schema is a fresh `format_version=1` (no prior v1 promise to break), and
existing console/markdown goldens stay byte-stable in normal mode.

### Added

- HTML report format (`--format html`) — self-contained HTML5 document
  with inline CSS, no external resources or JavaScript; contract pinned
  in `spec/report-html.md` and validated via
  `tests/validate_html_reports.py`.
- JSON report format (`--format json`) with versioned schema
  (`spec/report-json.schema.json`, Draft 2020-12, `format_version: 1`)
  and a per-report manifest under `tests/e2e/testdata/m5/json-reports/`.
- Graphviz DOT report format (`--format dot`) for visualising
  top-rankings, hotspots and impact relationships; contract pinned in
  `spec/report-dot.md`, validated via `tests/validate_dot_reports.py`
  plus optional `dot -Tsvg`/`-Tplain`/`-Tjson` gates when Graphviz is
  installed.
- Atomic file output via `--output <path>` for `markdown`, `json`,
  `dot` and `html`; existing target files survive render, write and
  replace failures. POSIX uses `rename`/`renameat`; Windows uses
  `ReplaceFileW`/`MoveFileExW` with `MOVEFILE_WRITE_THROUGH`.
- Command-local `--quiet` and `--verbose` flags on `analyze` and
  `impact` (mutually exclusive); contract pinned in
  `docs/planning/plan-M5-1-5.md` and validated via
  `tests/validate_verbosity_reports.py`.
- Top-level `--version` flag that prints the resolved app version
  without subcommand initialisation.
- Linux release archive
  (`cmake-xray_<version>_linux_x86_64.tar.gz` plus `*.sha256` sidecar),
  reproducibly built via `scripts/build-release-archive.sh` with a
  fixed `SOURCE_DATE_EPOCH`.
- OCI runtime image published to GHCR (`ghcr.io/<owner>/cmake-xray`)
  with versioned tag plus `:latest` for non-prerelease versions, built
  and pushed via `scripts/oci-image-publish.sh`.
- Release tag validator (`scripts/validate-release-tag.sh`) for the
  SemVer + prerelease-suffix tag pattern; build metadata (`+...`) is
  rejected.
- Release-asset allowlist guard (`scripts/release-allowlist.sh`)
  blocking macOS/Windows preview artefacts in the official Linux
  release-asset list.
- Release dry-run orchestrator (`scripts/release-dry-run.sh`) using a
  fake `gh` and a local `registry:2` container; covers nine scenarios
  including the AP 1.7 negative case for a darwin/windows archive in
  the asset path.
- Native CI matrix in `.github/workflows/build.yml` for
  `linux-x86_64`, `macos-arm64` and `windows-x86_64`; Windows
  additionally runs `scripts/platform-smoke.ps1` as the
  `native_powershell` Pflichtmodus next to the MSYS-Bash path.
- Toolchain fail-fast gate (`cmake/ToolchainMinimums.cmake`) backed by
  `tests/platform/toolchain-minimums.json` (CMake 3.20, GCC 10,
  Clang 12, AppleClang 13, MSVC 19.29) plus three CTest smokes
  (`toolchain_minimums_accepts_current_gnu` and two `WILL_FAIL`
  negatives).
- Plattformstatus classification (`supported` / `validated_smoke` /
  `known_limited`) consistently in README, `docs/user/quality.md`,
  `docs/user/releasing.md` and `docs/user/guide.md`. Linux x86_64 is
  `supported`; macOS arm64 and Windows x86_64 are `known_limited`
  pending external Branch-Protection and a green CI audit.
- Synthetic UNC and Extended-Length path tests across DOT, HTML, JSON
  adapters and atomic-writer (Windows-conditional via `#ifdef _WIN32`).
- `docs/examples/` drift gate covering all four artefact formats
  (console, markdown, JSON, DOT, HTML): CTest
  `doc_examples_validation` via `tests/validate_doc_examples.py`
  against `docs/examples/manifest.txt` SHA-256 hashes, per-format
  validators, and against the current generator output via
  `docs/examples/generation-spec.json` and the validator's
  `--binary` mode. The drift check normalises CRLF and DOT
  `unique_key` host paths so the gate is byte-stable across
  Linux, macOS and Windows runners.
- `docs/examples/` host-path portability check
  (`scripts/verify-doc-examples-portability.sh`, CTest
  `doc_examples_host_portability`, plus a dedicated
  `docs-examples-portability` job in
  `.github/workflows/build.yml`) that runs the drift gate under a
  divergent bind-mount prefix so host-path leakage in the binary
  output is caught before the native matrix runs.
- File-API performance baseline in `docs/user/performance.md` plus
  versioned reply fixtures under
  `tests/reference/scale_*/build/.cmake/api/v1/reply/`. Regenerate
  via `tests/reference/generate_file_api_fixtures.sh`; the manifest
  `tests/reference/file-api-performance-manifest.json` documents
  CMake version, generator and reply paths.
- Manual platform smoke checklist
  ([`docs/user/platform-smoke-checklist.md`](docs/user/platform-smoke-checklist.md))
  mirroring the Required-Check sequence for local platform
  verification.

### Changed

- Project and application version bumped to `1.2.0`. Release builds
  resolve `XRAY_APP_VERSION` from the validated tag without a leading
  `v`; `XRAY_VERSION_SUFFIX` stays available for local non-release
  builds, and setting both at once is rejected at configure time.
- Console and markdown reporters keep their byte-stable v1.1.0 output
  in normal mode; AP M5-1.1 input-provenance fields surface only in
  JSON, DOT and HTML.
- README, `docs/user/guide.md` and `docs/user/releasing.md` describe user
  invocations via released artefacts and the OCI image; internal
  build paths (`./build/cmake-xray`) are reserved for `docs/user/performance.md`
  measurement commands.

### Fixed

- `impact --format json` and `impact --format json --output <path>`
  now reject `unresolved_file_api_source_root` like DOT and HTML
  do. Previously the JSON path produced a report on stdout (or at
  the target path) and exited `0`, contradicting the JSON v1
  "Textfehler ohne JSON-Report" contract pinned in
  `spec/report-json.md`.
- `docs/user/guide.md` OCI tag references aligned with the AP M5-1.6
  release contract: container pulls use `X.Y.Z` (no leading `v`),
  consistent with `README.md`, `scripts/oci-image-publish.sh` and
  the `release.yml` push step.

### Platform status (M5)

| Platform | Status |
|---|---|
| Linux x86_64 | `supported` |
| macOS arm64 | `known_limited` |
| Windows x86_64 | `known_limited` |

`known_limited` rests on two external preconditions per
[`docs/user/quality.md`](docs/user/quality.md) "Plattformstatus (AP M5-1.7)":
the Branch-Protection of the repository must pin the
`Native (...)`-Required-Checks for the affected platforms, and the
green CI run on those platforms must be auditable. Both conditions
are external to this repository and not gated by AP 1.8.

## [1.1.0] - 2026-04-24

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
- `docs/user/performance.md` with measured baseline artifacts and explicit NF-04/NF-05 evaluation for the MVP path

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
