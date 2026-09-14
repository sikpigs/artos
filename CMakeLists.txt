cmake_minimum_required(VERSION 3.24)

project(artos_firmware LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# clangd searches the source tree for compile_commands.json, while CMake
# generates it in the active preset's binary directory. A symbolic link can be
# created before the database itself is generated and always points at the
# database for the most recently configured preset.
file(CREATE_LINK
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    SYMBOLIC
)

include(cmake/kconfig.cmake)
# The current SoCs use ARM cores; each core owns its flags, headers and sources.
set(ARCH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/arch/arm/${CONFIG_CPU}")
include("${ARCH_DIR}/cpu.cmake")
set(FIRMWARE_TARGET "${CONFIG_BOARD}_baremetal")
# Retire the old cache selector: Kconfig is the sole source of board selection.
if(DEFINED BSP)
    if(NOT BSP STREQUAL CONFIG_BSP)
        message(FATAL_ERROR "BSP selection moved to Kconfig; remove -DBSP and use menuconfig")
    endif()
    unset(BSP CACHE)
endif()
add_library(firmware_headers INTERFACE)
target_include_directories(firmware_headers INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ARCH_DIR}/include
)
add_subdirectory(bsp)

add_executable(${FIRMWARE_TARGET})

add_subdirectory(app)
add_subdirectory("${ARCH_DIR}" arch)
add_subdirectory(runtime)
add_subdirectory(kernel)

if(CONFIG_CONSOLE)
    add_subdirectory(modules/console)
    target_link_libraries(${FIRMWARE_TARGET} PRIVATE console::console)
endif()

if(CONFIG_MONITOR)
    add_subdirectory(modules/monitor)
    target_link_libraries(${FIRMWARE_TARGET} PRIVATE monitor::monitor)
endif()
target_link_libraries(${FIRMWARE_TARGET} PRIVATE bsp::board)

target_compile_options(${FIRMWARE_TARGET} PRIVATE
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g3>
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wdouble-promotion
    -ffunction-sections
    -fdata-sections
)

target_link_options(${FIRMWARE_TARGET} PRIVATE
    -nostartfiles
    # Firmware does not execute code on the stack; libnosys objects may lack
    # .note.GNU-stack metadata, so declare this explicitly at link time.
    -Wl,-z,noexecstack
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_TARGET}.map
    --specs=nano.specs
    --specs=nosys.specs
)

set_target_properties(${FIRMWARE_TARGET} PROPERTIES LINKER_LANGUAGE CXX SUFFIX ".elf")

add_custom_command(TARGET ${FIRMWARE_TARGET} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${FIRMWARE_TARGET}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${FIRMWARE_TARGET}>
            ${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_TARGET}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${FIRMWARE_TARGET}>
            ${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_TARGET}.bin
    COMMENT "Creating HEX and BIN firmware images"
)

# OpenOCD is optional for building firmware. Override with -DOPENOCD_EXECUTABLE=...
# when it is installed outside PATH.
find_program(OPENOCD_EXECUTABLE NAMES openocd)
if(OPENOCD_EXECUTABLE)
    add_custom_target(flash
        COMMAND "${OPENOCD_EXECUTABLE}" -f "${BSP_OPENOCD_CONFIG}"
            -c "program {$<TARGET_FILE:${FIRMWARE_TARGET}>} verify reset exit"
        DEPENDS ${FIRMWARE_TARGET}
        USES_TERMINAL
        VERBATIM
        COMMENT "Flashing and verifying ${CONFIG_BOARD} via ST-LINK"
    )
    add_custom_target(debug-server
        COMMAND "${OPENOCD_EXECUTABLE}" -f "${BSP_OPENOCD_CONFIG}"
        USES_TERMINAL
        VERBATIM
        COMMENT "Starting OpenOCD; connect GDB to localhost:3333 (Ctrl-C to stop)"
    )
endif()

# Compile the configuration contract and otherwise-unused thread API on demand.
add_library(configuration-checks OBJECT EXCLUDE_FROM_ALL tests/configuration.cpp tests/fault_capture.cpp tests/board_contract.cpp)
target_link_libraries(configuration-checks PRIVATE bsp::headers)
target_compile_options(configuration-checks PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(regmap-layout-checks OBJECT EXCLUDE_FROM_ALL tests/regmap.cpp tests/regmap_f439.cpp)
target_link_libraries(regmap-layout-checks PRIVATE firmware_headers)
target_include_directories(regmap-layout-checks PRIVATE
    ${CMAKE_SOURCE_DIR}/bsp/drivers/include
    ${CMAKE_SOURCE_DIR}/bsp/soc/st/stm32f4/stm32f411/include
    ${CMAKE_SOURCE_DIR}/bsp/soc/st/stm32f4/stm32f439/include)
target_compile_options(regmap-layout-checks PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_custom_target(regmap-checks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_svd_to_regmap.py"
    DEPENDS configuration-checks regmap-layout-checks
    VERBATIM COMMENT "Check SVD normalization, corrections, and generated output")

add_custom_target(bsp-checks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_firmware.py"
        --elf "$<TARGET_FILE:${FIRMWARE_TARGET}>" --board "${CONFIG_BOARD}"
        --objcopy "${CMAKE_OBJCOPY}" --nm "${CMAKE_NM}"
    DEPENDS ${FIRMWARE_TARGET} configuration-checks
    VERBATIM COMMENT "Verify linked board memory and interrupt vectors")

add_custom_target(kconfig-checks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kconfig.py"
    VERBATIM COMMENT "Check Kconfig generation and validation")

add_custom_target(devicetree-checks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dtb_to_board.py"
    DEPENDS configuration-checks
    VERBATIM COMMENT "Check DTB bindings, validation and generated board headers")
