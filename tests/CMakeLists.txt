add_executable(
    xray_tests
    test_main.cpp
    hexagon/test_project_analyzer.cpp
    adapters/test_port_wiring.cpp
)

target_link_libraries(
    xray_tests
    PRIVATE
        doctest::doctest
        xray_hexagon
        xray_adapters
)

add_test(NAME xray_tests COMMAND xray_tests)

add_test(
    NAME hexagon_boundary_check
    COMMAND bash ${PROJECT_SOURCE_DIR}/cmake/check_hexagon_boundary.sh ${PROJECT_SOURCE_DIR}/src/hexagon
)
