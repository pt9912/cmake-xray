add_executable(
    xray_tests
    test_main.cpp
    hexagon/test_project_analyzer.cpp
    adapters/test_port_wiring.cpp
    adapters/test_compile_commands_json.cpp
    e2e/test_cli.cpp
)

target_link_libraries(
    xray_tests
    PRIVATE
        doctest::doctest
        xray_hexagon
        xray_adapters
)

add_test(NAME xray_tests COMMAND xray_tests)
set_tests_properties(xray_tests PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

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
