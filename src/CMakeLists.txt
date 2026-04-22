add_subdirectory(hexagon)
add_subdirectory(adapters)

add_executable(cmake-xray main.cpp)
target_link_libraries(cmake-xray PRIVATE xray_hexagon xray_adapters)
xray_enable_coverage(cmake-xray)
