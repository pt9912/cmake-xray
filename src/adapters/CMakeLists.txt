add_library(
    xray_adapters
    cli/atomic_report_writer.cpp
    cli/cli_adapter.cpp
    cli/cli_console_renderers.cpp
    input/analysis_json_reader.cpp
    input/cmake_file_api_adapter.cpp
    input/compile_commands_json_adapter.cpp
    input/source_parsing_include_adapter.cpp
    output/console_report_adapter.cpp
    output/dot_report_adapter.cpp
    output/html_report_adapter.cpp
    output/json_compare_adapter.cpp
    output/json_report_adapter.cpp
    output/markdown_report_adapter.cpp
    output/target_display_support.cpp
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
xray_enable_coverage(xray_adapters)
