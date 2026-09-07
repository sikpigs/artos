# Like the BSP, this module supplies sources to the final firmware target so
# they inherit its CPU flags, warnings, optimization, and section settings.
add_library(monitor INTERFACE)
add_library(monitor::monitor ALIAS monitor)
target_include_directories(monitor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(monitor INTERFACE bsp::headers)
target_sources(monitor INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wozmon.s
)
