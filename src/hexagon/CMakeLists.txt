add_library(
    xray_hexagon
    services/diagnostic_support.cpp
    services/analysis_support.cpp
    services/impact_analyzer.cpp
    services/project_analyzer.cpp
    services/report_generator.cpp
)

target_include_directories(xray_hexagon
    PUBLIC    ${PROJECT_SOURCE_DIR}/src          # consumers include via hexagon/...
    PRIVATE   ${PROJECT_SOURCE_DIR}/src/hexagon  # hexagon code only sees its own headers
)
target_compile_features(xray_hexagon PUBLIC cxx_std_20)
xray_enable_coverage(xray_hexagon)
