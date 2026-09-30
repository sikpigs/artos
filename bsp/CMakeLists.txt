# Selected board -> exact MCU variant -> SoC register definitions and CPU support.
set(SOC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/soc/${CONFIG_SOC_PATH}")
include("${SOC_DIR}/variants/${CONFIG_MCU_VARIANT}.cmake")
configure_file(mcu.h.in "${CMAKE_BINARY_DIR}/generated/mcu.h" @ONLY)

add_library(stm32_drivers INTERFACE)
add_library(stm32::drivers ALIAS stm32_drivers)
target_include_directories(stm32_drivers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/drivers/include")
target_link_libraries(stm32_drivers INTERFACE firmware_headers)

add_library(soc_headers INTERFACE)
add_library(soc::headers ALIAS soc_headers)
target_include_directories(soc_headers INTERFACE "${SOC_DIR}/include")
target_link_libraries(soc_headers INTERFACE stm32::drivers)

add_custom_target(svd-regmap-check
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/svd_to_regmap.py" --soc-dir "${SOC_DIR}" --check
    VERBATIM COMMENT "Check selected SoC register definitions")
add_dependencies(soc_headers svd-regmap-check)
add_custom_target(regenerate-regmap
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/svd_to_regmap.py" --soc-dir "${SOC_DIR}"
    VERBATIM COMMENT "Regenerate selected SoC register definitions")

add_library(bsp_headers INTERFACE)
add_library(bsp::headers ALIAS bsp_headers)
find_program(DTC_EXECUTABLE dtc REQUIRED)
find_program(FDTGET_EXECUTABLE fdtget REQUIRED)
set(BOARD_DTS "${CMAKE_CURRENT_SOURCE_DIR}/boards/${CONFIG_BSP}/board.dts")
set(BOARD_GENERATED "${CMAKE_BINARY_DIR}/generated/board")
file(MAKE_DIRECTORY "${BOARD_GENERATED}")
add_custom_command(
    OUTPUT "${BOARD_GENERATED}/board.dtb" "${BOARD_GENERATED}/board.h" "${BOARD_GENERATED}/board_devices.h"
    COMMAND "${DTC_EXECUTABLE}" -I dts -O dtb -i "${SOC_DIR}/dts"
        -d "${BOARD_GENERATED}/board.d" -o "${BOARD_GENERATED}/board.dtb" "${BOARD_DTS}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/dtb_to_board.py"
        --dtb "${BOARD_GENERATED}/board.dtb" --bindings "${SOC_DIR}/dts/bindings.json"
        --board "${CONFIG_BOARD}" --output "${BOARD_GENERATED}" --fdtget "${FDTGET_EXECUTABLE}"
    DEPENDS "${BOARD_DTS}" "${SOC_DIR}/dts/${CONFIG_SOC}.dtsi" "${SOC_DIR}/dts/bindings.json"
        "${PROJECT_SOURCE_DIR}/scripts/dtb_to_board.py"
        "${PROJECT_SOURCE_DIR}/scripts/templates/board_devices.h.in"
    DEPFILE "${BOARD_GENERATED}/board.d"
    VERBATIM COMMENT "Compile board DTS and generate board headers")
add_custom_target(board-headers DEPENDS "${BOARD_GENERATED}/board.h" "${BOARD_GENERATED}/board_devices.h")
add_dependencies(bsp_headers board-headers)
target_include_directories(bsp_headers INTERFACE "${BOARD_GENERATED}")
target_link_libraries(bsp_headers INTERFACE soc::headers)
add_library(bsp INTERFACE)
add_library(bsp::board ALIAS bsp)
if(NOT DEFINED SOC_STOPWATCH_SOURCE OR NOT EXISTS "${SOC_STOPWATCH_SOURCE}")
    message(FATAL_ERROR "Selected SoC must provide SOC_STOPWATCH_SOURCE")
endif()
target_sources(bsp INTERFACE "${SOC_STOPWATCH_SOURCE}")
target_link_libraries(bsp INTERFACE bsp::headers)
target_sources(bsp INTERFACE "${SOC_DIR}/startup/vectors.s")
set(linker_script "${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_MCU_VARIANT}_FLASH.ld")
configure_file(linker/cortex_m.ld.in "${linker_script}" @ONLY)
target_link_options(bsp INTERFACE "-T${linker_script}")
set_property(TARGET bsp PROPERTY INTERFACE_LINK_DEPENDS "${linker_script}")

add_subdirectory("boards/${CONFIG_BSP}")
set(BSP_OPENOCD_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/boards/${CONFIG_BSP}/openocd.cfg" PARENT_SCOPE)
