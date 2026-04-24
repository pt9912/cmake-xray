add_executable(
    xray_tests
    test_main.cpp
    hexagon/test_analysis_support.cpp
    hexagon/test_project_analyzer.cpp
    hexagon/test_impact_analyzer.cpp
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
)

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
