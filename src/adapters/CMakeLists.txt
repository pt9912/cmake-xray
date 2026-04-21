add_library(
    xray_adapters
    cli/placeholder_cli_adapter.cpp
    input/json_dependency_probe.cpp
    output/placeholder_report_adapter.cpp
)

target_include_directories(xray_adapters PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(
    xray_adapters
    PRIVATE
        xray_hexagon
        CLI11::CLI11
        nlohmann_json::nlohmann_json
)
target_compile_features(xray_adapters PUBLIC cxx_std_20)
