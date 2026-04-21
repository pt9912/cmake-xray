add_library(
    xray_hexagon
    services/startup_message_service.cpp
)

target_include_directories(xray_hexagon PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_features(xray_hexagon PUBLIC cxx_std_20)
