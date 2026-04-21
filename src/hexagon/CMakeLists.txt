add_library(
    xray_hexagon
    services/startup_message_service.cpp
)

target_include_directories(xray_hexagon
    INTERFACE ${PROJECT_SOURCE_DIR}/src          # consumers include via hexagon/...
    PRIVATE   ${PROJECT_SOURCE_DIR}/src/hexagon  # hexagon code only sees its own headers
)
target_compile_features(xray_hexagon PUBLIC cxx_std_20)
