add_executable(
    xray_tests
    test_main.cpp
    hexagon/test_analysis_support.cpp
    hexagon/test_project_analyzer.cpp
    hexagon/test_impact_analyzer.cpp
    adapters/test_atomic_report_writer.cpp
    adapters/test_port_wiring.cpp
    adapters/test_cmake_file_api_adapter.cpp
    adapters/test_compile_commands_json.cpp
    adapters/test_console_report_adapter.cpp
    adapters/test_markdown_report_adapter.cpp
    adapters/test_source_parsing_include_adapter.cpp
    e2e/test_cli.cpp
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
    XRAY_REPORT_JSON_SCHEMA_PATH="${PROJECT_SOURCE_DIR}/docs/report-json.schema.json"
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

find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
    add_test(
        NAME hexagon_boundary_check
        COMMAND bash ${PROJECT_SOURCE_DIR}/cmake/check_hexagon_boundary.sh ${PROJECT_SOURCE_DIR}/src/hexagon
    )

    add_test(
        NAME e2e_binary
        COMMAND bash ${PROJECT_SOURCE_DIR}/tests/e2e/run_e2e.sh
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
            --schema ${PROJECT_SOURCE_DIR}/docs/report-json.schema.json
            --reports-dir ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/json-reports
            --manifest ${PROJECT_SOURCE_DIR}/tests/e2e/testdata/m5/json-reports/manifest.txt
)
