# Compile-only consumers can use headers without receiving startup or link flags.
add_library(bsp_headers INTERFACE)
add_library(bsp::headers ALIAS bsp_headers)
target_include_directories(bsp_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bsp_headers INTERFACE firmware_headers)

# Interface sources are compiled directly into the firmware target. In particular,
# startup vectors and IRQ handlers must not be lost through archive extraction.
add_library(bsp INTERFACE)
add_library(bsp::board ALIAS bsp)
target_link_libraries(bsp INTERFACE bsp::headers)
target_sources(bsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/board.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/irq.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_stm32f411xe.s
)
set(bsp_linker_script ${CMAKE_CURRENT_BINARY_DIR}/STM32F411RETx_FLASH.ld)
configure_file(linker/STM32F411RETx_FLASH.ld.in "${bsp_linker_script}" @ONLY)
target_link_options(bsp INTERFACE "-T${bsp_linker_script}")
set_property(TARGET bsp PROPERTY INTERFACE_LINK_DEPENDS "${bsp_linker_script}")
set(BSP_OPENOCD_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/openocd.cfg" PARENT_SCOPE)
