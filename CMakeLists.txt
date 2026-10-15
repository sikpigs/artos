cmake_minimum_required(VERSION 3.24)

option(ARTOS_HOST_TESTS "Configure the host-native CTest project" OFF)
if(ARTOS_HOST_TESTS)
    project(artos_host_tests LANGUAGES CXX)
    enable_testing()
    add_subdirectory(tests/host)
    return()
endif()

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

set(ARTOS_OUTPUT_NAME "artos" CACHE STRING "Base name for firmware artifacts")
if(NOT ARTOS_OUTPUT_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9._-]*$")
    message(FATAL_ERROR
        "ARTOS_OUTPUT_NAME must start with an alphanumeric character and contain only letters, numbers, '.', '_' or '-'")
endif()
set(FIRMWARE_OUTPUT_BASE "${CMAKE_CURRENT_BINARY_DIR}/${ARTOS_OUTPUT_NAME}")

add_library(firmware_headers INTERFACE)
target_include_directories(firmware_headers INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ARCH_DIR}/include
)

# Port contracts are shared by the kernel and selected port implementations,
# but are not part of the public application header directory.
add_library(port_headers INTERFACE)
target_include_directories(port_headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/port/include
)
add_subdirectory(bsp)

add_executable(firmware)
target_link_libraries(firmware PRIVATE port_headers)

add_subdirectory(app)
add_subdirectory("${ARCH_DIR}" arch)
add_subdirectory(runtime)
add_subdirectory(kernel)

if(CONFIG_CONSOLE)
    add_subdirectory(modules/console)
    target_link_libraries(firmware PRIVATE console::console)
endif()

if(CONFIG_MONITOR)
    add_subdirectory(modules/monitor)
    target_link_libraries(firmware PRIVATE monitor::monitor)
endif()
target_link_libraries(firmware PRIVATE bsp::board)

target_compile_options(firmware PRIVATE
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

target_link_options(firmware PRIVATE
    -nostartfiles
    # Firmware does not execute code on the stack; libnosys objects may lack
    # .note.GNU-stack metadata, so declare this explicitly at link time.
    -Wl,-z,noexecstack
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${FIRMWARE_OUTPUT_BASE}.map
    --specs=nano.specs
    --specs=nosys.specs
)

set_target_properties(firmware PROPERTIES
    LINKER_LANGUAGE CXX
    OUTPUT_NAME "${ARTOS_OUTPUT_NAME}"
    SUFFIX ".elf"
)

add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware>
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:firmware>
            ${FIRMWARE_OUTPUT_BASE}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:firmware>
            ${FIRMWARE_OUTPUT_BASE}.bin
    COMMENT "Creating HEX and BIN firmware images"
)

# OpenOCD is optional for building firmware. Override with -DOPENOCD_EXECUTABLE=...
# when it is installed outside PATH.
find_program(OPENOCD_EXECUTABLE NAMES openocd)
set(OPENOCD_SERIAL "" CACHE STRING "ST-LINK USB serial number (empty selects any probe)")
set(openocd_probe_args)
if(OPENOCD_SERIAL)
    if(NOT OPENOCD_SERIAL MATCHES "^[a-zA-Z0-9]+$")
        message(FATAL_ERROR "OPENOCD_SERIAL must be an alphanumeric USB serial number")
    endif()
    list(APPEND openocd_probe_args -c "adapter serial ${OPENOCD_SERIAL}")
endif()
if(OPENOCD_EXECUTABLE)
    add_custom_target(flash
        COMMAND "${OPENOCD_EXECUTABLE}" -f "${BSP_OPENOCD_CONFIG}"
            ${openocd_probe_args}
            -c "program {$<TARGET_FILE:firmware>} verify reset exit"
        DEPENDS firmware
        USES_TERMINAL
        VERBATIM
        COMMENT "Flashing and verifying ${CONFIG_BOARD} via ST-LINK"
    )
    add_custom_target(debug-server
        COMMAND "${OPENOCD_EXECUTABLE}" -f "${BSP_OPENOCD_CONFIG}"
            ${openocd_probe_args}
        USES_TERMINAL
        VERBATIM
        COMMENT "Starting OpenOCD; connect GDB to localhost:3333 (Ctrl-C to stop)"
    )
endif()

# Compile the configuration contract and otherwise-unused thread API on demand.
add_library(configuration-checks OBJECT EXCLUDE_FROM_ALL tests/configuration.cpp tests/fault_capture.cpp tests/board_contract.cpp)
target_link_libraries(configuration-checks PRIVATE bsp::headers port_headers)
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
        --elf "$<TARGET_FILE:firmware>" --board "${CONFIG_BOARD}"
        --objcopy "${CMAKE_OBJCOPY}" --nm "${CMAKE_NM}"
    DEPENDS firmware configuration-checks
    VERBATIM COMMENT "Verify linked board memory and interrupt vectors")

add_custom_target(kconfig-checks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kconfig.py"
    VERBATIM COMMENT "Check Kconfig generation and validation")

add_custom_target(devicetree-checks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dtb_to_board.py"
    DEPENDS configuration-checks
    VERBATIM COMMENT "Check DTB bindings, validation and generated board headers")
