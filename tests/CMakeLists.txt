add_executable(
    xray_tests
    test_main.cpp
    hexagon/test_analysis_support.cpp
    hexagon/test_compile_db_fingerprint.cpp
    hexagon/test_compare_service.cpp
    hexagon/test_project_analyzer.cpp
    hexagon/test_impact_analyzer.cpp
    hexagon/test_target_graph_support.cpp
    hexagon/test_target_graph_traversal.cpp
    adapters/test_atomic_report_writer.cpp
    adapters/test_analysis_json_reader.cpp
    adapters/test_cli_console_renderers.cpp
    adapters/test_port_wiring.cpp
    adapters/test_cmake_file_api_adapter.cpp
    adapters/test_compile_commands_json.cpp
    adapters/test_console_report_adapter.cpp
    adapters/test_dot_report_adapter.cpp
    adapters/test_html_report_adapter.cpp
    adapters/test_json_compare_adapter.cpp
    adapters/test_json_report_adapter.cpp
    adapters/test_markdown_report_adapter.cpp
    adapters/test_source_parsing_include_adapter.cpp
    e2e/test_cli.cpp
    e2e/test_cli_render_errors.cpp
    e2e/test_cli_verbosity.cpp
)

target_link_libraries(
    xray_tests
    PRIVATE
        doctest::doctest
        xray_hexagon
        xray_adapters
        nlohmann_json::nlohmann_json
)

# Pass the absolute schema path to the format-version consistency test so it
# stays cwd-independent even if a future change drops the WORKING_DIRECTORY
# property below.
target_compile_definitions(
    xray_tests PRIVATE
    XRAY_REPORT_JSON_SCHEMA_PATH="${PROJECT_SOURCE_DIR}/spec/report-json.schema.json"
)

if(MSVC)
    target_compile_options(xray_tests PRIVATE /FI"ostream")
endif()

find_library(CAP_LIBRARY cap)
if(CAP_LIBRARY)
    target_link_libraries(xray_tests PRIVATE cap)
    target_compile_definitions(xray_tests PRIVATE XRAY_HAS_LIBCAP)
endif()

xray_enable_coverage(xray_tests)

add_test(NAME xray_tests COMMAND xray_tests)
set_tests_properties(xray_tests PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# AP M5-1.7 Tranche A follow-up: positive plus negative smokes for the
# toolchain-minimums fail-fast gate in cmake/ToolchainMinimums.cmake. The
# wrapper scripts run cmake -P in script mode, fake CMAKE_CXX_COMPILER_ID
# and CMAKE_CXX_COMPILER_VERSION, and include the production module so a
# regression in the gate logic (silent skip, wrong key, broken JSON path
# resolution) surfaces in CI instead of only when a real host falls
# below the minimum.
add_test(
    NAME toolchain_minimums_accepts_current_gnu
    COMMAND ${CMAKE_COMMAND} -P
            ${PROJECT_SOURCE_DIR}/tests/platform/cmake/test_toolchain_minimums_accepts_current_gnu.cmake
)

add_test(
    NAME toolchain_minimums_rejects_old_gnu
    COMMAND ${CMAKE_COMMAND} -P
            ${PROJECT_SOURCE_DIR}/tests/platform/cmake/test_toolchain_minimums_rejects_old_gnu.cmake
)
set_tests_properties(toolchain_minimums_rejects_old_gnu PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME toolchain_minimums_rejects_unknown_compiler_id
    COMMAND ${CMAKE_COMMAND} -P
            ${PROJECT_SOURCE_DIR}/tests/platform/cmake/test_toolchain_minimums_rejects_unknown_compiler_id.cmake
)
set_tests_properties(toolchain_minimums_rejects_unknown_compiler_id PROPERTIES WILL_FAIL TRUE)

find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
    add_test(
        NAME hexagon_boundary_check
        COMMAND bash ${PROJECT_SOURCE_DIR}/cmake/check_hexagon_boundary.sh ${PROJECT_SOURCE_DIR}/src/hexagon
    )

    # AP M5-1.6 Tranche A: SemVer-Tag-Validator-Smokes. Pinnt 4 Positiv-
    # und 12 Negativfaelle aus plan-M5-1-6.md "Smoke- und Validierungstests"
    # plus den Usage-Error-Pfad (Exit 2).
    add_test(
        NAME release_tag_validation
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/release/test_validate_release_tag.sh
    )

    # AP M5-1.6 Tranche B: Linux release archive reproducibility smoke. Builds
    # the archive twice via scripts/build-release-archive.sh and compares
    # archive checksum, member listing, sidecar and embedded binary checksum
    # byte-for-byte. Linux-only; macOS/Windows hosts get SKIP.
    # WORKING_DIRECTORY is the project root so the smoke can locate
    # build/_deps as the shared FetchContent cache.
    add_test(
        NAME release_archive_reproducibility
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/release/test_release_archive_repro.sh
    )
    set_tests_properties(release_archive_reproducibility PROPERTIES
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

    # AP M5-1.6 Tranche C: OCI runtime image smoke. Builds the runtime
    # image locally via scripts/oci-image-publish.sh build, smoke-tests
    # --help and --version inside the container, pins the OCI label
    # against the build-arg version, and verifies the latest-refusal
    # contract for prerelease versions. SKIPs cleanly when docker is
    # missing or the host is not Linux.
    add_test(
        NAME oci_image_smoke
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/release/test_oci_image_smoke.sh
    )
    set_tests_properties(oci_image_smoke PROPERTIES
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

    # AP M5-1.6 Tranche D.1: release dry-run smoke. Drives the full
    # release sequence (archive + OCI + GitHub release) against fake
    # publishers (local registry:2 container plus the fake-gh wrapper).
    # Runs the orchestrator inside the cmake-xray:release-dry-run image
    # so cmake/jq/curl/docker-cli are available; SKIPs cleanly when
    # docker is missing on the host or the host is not Linux.
    add_test(
        NAME release_dry_run
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/release/test_release_dry_run.sh
    )
    set_tests_properties(release_dry_run PROPERTIES
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

    # AP M5-1.6 Tranche E: release-allowlist guard tests. Pinnt die
    # offizielle Linux-Asset-Liste, lehnt darwin/windows-Preview-Artefakte
    # und unbekannte Dateien ab und verifiziert die Usage-Error-Pfade.
    add_test(
        NAME release_allowlist
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/release/test_release_allowlist.sh
    )
    set_tests_properties(release_allowlist PROPERTIES
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

    # AP M5-1.5 Tranche D Folge-Refactor: split the monolithic run_e2e.sh
    # into focused sub-suites and register each as its own CTest entry so
    # `ctest --parallel` schedules them concurrently. run_e2e_lib.sh holds
    # the shared assert_* and validator helpers; each sub-script sources it
    # before running its own subset and reports its own failure count.
    add_test(
        NAME e2e_binary_smoke
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/e2e/run_e2e_smoke.sh
                $<TARGET_FILE:cmake-xray>
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata
    )

    add_test(
        NAME e2e_binary_artifacts
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/e2e/run_e2e_artifacts.sh
                $<TARGET_FILE:cmake-xray>
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata
    )

    add_test(
        NAME e2e_binary_verbosity
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/e2e/run_e2e_verbosity.sh
                $<TARGET_FILE:cmake-xray>
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata
    )
endif()

# AP M5-1.2 Tranche A: schema-syntax + manifest gate for the JSON report
# contract. The validator script fails loud (exit 2) if the jsonschema Python
# dependency is missing; CTest must never silently skip this gate.
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(
    NAME report_json_schema_validation
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/json-reports
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/json-reports/manifest.txt
)

# AP M6-1.2 Tranche A.1: M6 dataset goldens validate against the v2 schema
# alongside the existing m5/ tree. Both directories carry only v2 goldens.
add_test(
    NAME report_json_schema_validation_m6
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/json-reports
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/json-reports/manifest.txt
)

add_test(
    NAME report_compare_schema_validation_m6
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-compare.schema.json
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/compare-reports/json
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/compare-reports/json/manifest.txt
)

# AP M6-1.2 Tranche A.1: schema-asymmetry negative gates. The v2 schema
# requires target_hubs in analyze and forbids it in impact via
# additionalProperties=false. Both negative samples must fail validation.
add_test(
    NAME report_json_schema_rejects_impact_with_target_hubs
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/json_impact_with_target_hubs_broken_sample.json
)
set_tests_properties(report_json_schema_rejects_impact_with_target_hubs PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME report_json_schema_rejects_analyze_without_target_hubs
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/json_analyze_without_target_hubs_broken_sample.json
)
set_tests_properties(report_json_schema_rejects_analyze_without_target_hubs PROPERTIES WILL_FAIL TRUE)

# AP M6-1.3 A.5 follow-up + AP M6-1.4 A.3 review fix: schema-rejection
# gates for the v4 prioritised impact contract (renamed from v3 when
# AP M6-1.4 A.3 bumped kReportFormatVersion). Each fixture deliberately
# violates exactly one documented invariant; WILL_FAIL inverts the
# schema-validator outcome so the gate fails loud whenever a regression
# makes the schema accept bytes the contract explicitly forbids.
add_test(
    NAME report_json_schema_rejects_format_version_two
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/json_schema_rejects_format_version_two_broken_sample.json
)
set_tests_properties(report_json_schema_rejects_format_version_two PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME report_json_schema_rejects_impact_v4_missing_prioritized
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/json_impact_v4_missing_prioritized_broken_sample.json
)
set_tests_properties(report_json_schema_rejects_impact_v4_missing_prioritized PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME report_json_schema_rejects_impact_v4_invalid_priority_class
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/json_impact_v4_invalid_priority_class_broken_sample.json
)
set_tests_properties(report_json_schema_rejects_impact_v4_invalid_priority_class PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME report_json_schema_rejects_impact_v4_graph_distance_out_of_range
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/json_impact_v4_graph_distance_out_of_range_broken_sample.json
)
set_tests_properties(report_json_schema_rejects_impact_v4_graph_distance_out_of_range PROPERTIES WILL_FAIL TRUE)

# AP M6-1.3 Finding 3 follow-up: companion negative tests for the
# cross-field invariants that the JSON Schema Draft 2020-12 dialect
# cannot express. The validator runs the cross-field check after
# successful schema validation, so these fixtures pass the schema
# but fail the invariant.
add_test(
    NAME report_json_cross_field_rejects_depth_effective_exceeds_requested
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/json_impact_v4_depth_effective_exceeds_requested_broken_sample.json
)
set_tests_properties(report_json_cross_field_rejects_depth_effective_exceeds_requested PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME report_json_cross_field_rejects_graph_distance_exceeds_requested
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_json_schema.py
            --schema ${PROJECT_SOURCE_DIR}/spec/report-json.schema.json
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/json_impact_v4_graph_distance_exceeds_requested_broken_sample.json
)
set_tests_properties(report_json_cross_field_rejects_graph_distance_exceeds_requested PROPERTIES WILL_FAIL TRUE)

# AP M5-1.3 Tranche A: DOT contract gates. The Python fallback uses only
# stdlib and runs in every environment with python3 on PATH. The Graphviz
# path runs in addition wherever the `dot` binary is installed (Docker
# toolchain via apt). Both gates share the same manifest and goldens; both
# fail loud if the manifest or a golden is malformed.
add_test(
    NAME dot_report_python_validation
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_dot_reports.py
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/dot-reports
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/dot-reports/manifest.txt
)

# AP M6-1.2 Tranche A.2: M6 dataset DOT goldens validate against the same
# stdlib parser as m5/dot-reports/.
add_test(
    NAME dot_report_python_validation_m6
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_dot_reports.py
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/dot-reports
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/dot-reports/manifest.txt
)

# Negative gate: feeding a deliberately broken DOT must fail validation.
# WILL_FAIL inverts the test outcome so a passing (exit 0) result is
# treated as a failure of the broken-DOT detector.
add_test(
    NAME dot_report_python_validation_rejects_broken_dot
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_dot_reports.py
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/dot_broken_sample.dot
)
set_tests_properties(dot_report_python_validation_rejects_broken_dot PROPERTIES WILL_FAIL TRUE)

# find_program runs at configure time. Installing graphviz after configure
# silently disables the gate; re-run `cmake -B build` to register the test.
# Docker stages bake graphviz into the toolchain layer so the gate is always
# active in the canonical CI paths.
find_program(DOT_PROGRAM dot)
if(DOT_PROGRAM AND BASH_PROGRAM)
    add_test(
        NAME dot_report_graphviz_validation
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/validate_dot_via_graphviz.sh
                ${DOT_PROGRAM}
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/dot-reports
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/dot-reports/manifest.txt
                svg
    )

    add_test(
        NAME dot_report_graphviz_validation_rejects_broken_dot
        COMMAND ${DOT_PROGRAM} -Tsvg ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/dot_broken_sample.dot
    )
    set_tests_properties(dot_report_graphviz_validation_rejects_broken_dot PROPERTIES WILL_FAIL TRUE)

    # AP M5-1.3 Tranche D: parallel format gates make sure goldens stay
    # parseable across the alternate Graphviz output formats users typically
    # pipe DOT through. -Tplain exercises the canonical text emitter
    # (different parser path from -Tsvg) and -Tjson exercises the structured
    # JSON emitter; both fail loud if a golden uses an attribute/value the
    # respective output backend cannot encode.
    add_test(
        NAME dot_report_graphviz_validation_plain
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/validate_dot_via_graphviz.sh
                ${DOT_PROGRAM}
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/dot-reports
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/dot-reports/manifest.txt
                plain
    )
    add_test(
        NAME dot_report_graphviz_validation_json
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/validate_dot_via_graphviz.sh
                ${DOT_PROGRAM}
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/dot-reports
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/dot-reports/manifest.txt
                json
    )

    # AP M6-1.2 Tranche A.2: M6 DOT goldens go through the same Graphviz
    # backends as the m5/ tree so target_dependency edges and external_target
    # nodes stay -Tsvg/-Tplain/-Tjson clean.
    add_test(
        NAME dot_report_graphviz_validation_m6
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/validate_dot_via_graphviz.sh
                ${DOT_PROGRAM}
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/dot-reports
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/dot-reports/manifest.txt
                svg
    )
    add_test(
        NAME dot_report_graphviz_validation_m6_plain
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/validate_dot_via_graphviz.sh
                ${DOT_PROGRAM}
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/dot-reports
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/dot-reports/manifest.txt
                plain
    )
    add_test(
        NAME dot_report_graphviz_validation_m6_json
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/validate_dot_via_graphviz.sh
                ${DOT_PROGRAM}
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/dot-reports
                ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/dot-reports/manifest.txt
                json
    )
endif()

# AP M5-1.4 Tranche D: optional HTML structure smoke. Pure-stdlib Python
# validator that mirrors the DOT manifest+content gate and locks the static
# HTML contract from spec/report-html.md (single h1/main, no script, no
# external resources, no inline event handlers, no HTML comments). Runs in
# every environment with python3 on PATH; no pip dependency.
add_test(
    NAME html_structure_validation
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_html_reports.py
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/html-reports
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/html-reports/manifest.txt
)

# AP M6-1.2 Tranche A.3: M6 dataset HTML goldens validate against the same
# stdlib structure smoke as m5/html-reports. Both directories carry only
# format_version=2 goldens; the m5/m6 split is dataset-scenario-driven.
add_test(
    NAME html_structure_validation_m6
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_html_reports.py
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/html-reports
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/html-reports/manifest.txt
)

# Negative gate: a deliberately broken HTML fixture must fail validation so
# we have evidence the smoke actually rejects violations rather than silently
# passing. WILL_FAIL inverts the test outcome.
add_test(
    NAME html_structure_validation_rejects_broken_html
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_html_reports.py
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/html_broken_sample.html
)
set_tests_properties(html_structure_validation_rejects_broken_html PROPERTIES WILL_FAIL TRUE)

# AP M6-1.2 Tranche A.3: manifest/directory parity gates for the M6
# Markdown and Console reference goldens. There is no dedicated structure
# validator for these formats (markdown/console are line-based text without
# a formal contract beyond docs/planning/plan-M6-1-2.md); the parity check stops a
# stray file or an out-of-date manifest from passing review unnoticed.
add_test(
    NAME markdown_manifest_validation_m6
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_manifest_parity.py
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/markdown-reports
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/markdown-reports/manifest.txt
            --suffix .md
)
add_test(
    NAME console_manifest_validation_m6
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_manifest_parity.py
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/console-reports
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m6/console-reports/manifest.txt
            --suffix .txt
)

# AP M5-1.5 Tranche B.1: verbosity manifest/parity gate. Stdlib-only Python
# validator covering the same manifest+content invariants the JSON/DOT/HTML
# gates already enforce: every Quiet/Verbose golden begins with a documented
# anchor line, ends with a single newline, and contains no doubled newlines.
add_test(
    NAME verbosity_manifest_validation
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_verbosity_reports.py
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/verbosity
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/verbosity/manifest.txt
)

add_test(
    NAME verbosity_manifest_validation_rejects_broken_sample
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_verbosity_reports.py
            --validate-file ${PROJECT_SOURCE_DIR}/tests/adapters/fixtures/console_quiet_broken_sample.txt
)
set_tests_properties(verbosity_manifest_validation_rejects_broken_sample PROPERTIES WILL_FAIL TRUE)

# AP M5-1.8 A.2 + A.5 fixup: doc-examples drift gate. plan-M5-1-8.md
# "Validierung, dass docs/examples/ keine driftenden Kopien enthaelt"
# verlangt einen verbindlichen CI-Pfad gegen Drift in docs/examples/,
# entweder via Vergleich gegen den aktuellen Generatorausgang oder
# via Bytevergleich gegen Goldens. tests/validate_doc_examples.py
# prueft Manifest-Paritaet, SHA-256-Stabilitaet, ruft fuer json/dot/
# html die jeweiligen Format-Validatoren auf und faehrt mit
# --binary zusaetzlich den Generator-Drift-Check: jedes Beispiel
# wird via cmake-xray + den in docs/examples/generation-spec.json
# fixierten Args neu erzeugt und gegen die committete Datei
# byte-genau verglichen.
add_test(
    NAME doc_examples_validation
    COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/validate_doc_examples.py
            --repo-root ${PROJECT_SOURCE_DIR}
            --binary $<TARGET_FILE:cmake-xray>
)

# AP M5-1.8 A.5 follow-up: host-portability gate for the docs/examples
# drift check is wired exclusively via the docs-examples-portability
# job in .github/workflows/build.yml. An earlier iteration registered
# scripts/verify-doc-examples-portability.sh as a CTest entry too,
# but that put a third docker-heavy test (alongside release_dry_run
# and oci_image_smoke) on the same Linux native matrix runner via
# `ctest --parallel`, which produced silent failures in
# release_dry_run scenario 2 due to docker resource contention
# (GHA Build run #25126107438). The standalone workflow job runs on
# its own runner without that contention.
#
# Locally the dev still runs the portability script directly via
# `bash scripts/verify-doc-examples-portability.sh`; the
# docs/user/platform-smoke-checklist.md "docs/examples Host-
# Portabilitaet" section documents this as the explicit step before
# claiming "lokal gruen".
