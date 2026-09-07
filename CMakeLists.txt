cmake_minimum_required(VERSION 3.24)

project(nucleo_f411re_baremetal LANGUAGES C CXX ASM)

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
)
add_subdirectory(bsp/${CONFIG_BSP})

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/init.cpp
    src/abort.cpp
    src/cxa_guard.cpp
    include/bits.h
    include/asm.h
    include/artos.h
    include/artos_port.h
    include/ports/cortex_m4.h
    include/mmio.h
)

if(CONFIG_MONITOR)
    add_subdirectory(modules/monitor)
    target_link_libraries(${PROJECT_NAME} PRIVATE monitor::monitor)
endif()
if(CONFIG_ARTOS)
    target_sources(${PROJECT_NAME} PRIVATE src/artos.cpp src/ports/cortex_m4.cpp)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE bsp::board)

target_compile_options(${PROJECT_NAME} PRIVATE
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

target_link_options(${PROJECT_NAME} PRIVATE
    -nostartfiles
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    --specs=nano.specs
    --specs=nosys.specs
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX SUFFIX ".elf")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMENT "Creating HEX and BIN firmware images"
)

# OpenOCD is optional for building firmware. Override with -DOPENOCD_EXECUTABLE=...
# when it is installed outside PATH.
find_program(OPENOCD_EXECUTABLE NAMES openocd)
if(OPENOCD_EXECUTABLE)
    add_custom_target(flash
        COMMAND "${OPENOCD_EXECUTABLE}" -f "${BSP_OPENOCD_CONFIG}"
            -c "program {$<TARGET_FILE:${PROJECT_NAME}>} verify reset exit"
        DEPENDS ${PROJECT_NAME}
        USES_TERMINAL
        VERBATIM
        COMMENT "Flashing and verifying NUCLEO-F411RE via ST-LINK"
    )
    add_custom_target(debug-server
        COMMAND "${OPENOCD_EXECUTABLE}" -f "${BSP_OPENOCD_CONFIG}"
        USES_TERMINAL
        VERBATIM
        COMMENT "Starting OpenOCD; connect GDB to localhost:3333 (Ctrl-C to stop)"
    )
endif()

# Compile the configuration contract and otherwise-unused thread API on demand.
add_library(configuration-checks OBJECT EXCLUDE_FROM_ALL tests/configuration.cpp)
target_link_libraries(configuration-checks PRIVATE bsp::headers)
target_compile_options(configuration-checks PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_custom_target(kconfig-checks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_kconfig.py"
    VERBATIM COMMENT "Check Kconfig generation and validation")
