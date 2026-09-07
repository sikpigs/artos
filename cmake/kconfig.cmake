# Prefer the documented project-local environment; allow explicit override.
if(NOT DEFINED Python3_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/.venv/bin/python")
    set(Python3_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/bin/python")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(KCONFIG_DEFCONFIG "${CMAKE_SOURCE_DIR}/configs/nucleo_f411re_defconfig"
    CACHE FILEPATH "Initial Kconfig seed (used only when build/.config is absent)")
get_filename_component(KCONFIG_DEFCONFIG "${KCONFIG_DEFCONFIG}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
set(kconfig_command "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/configure.py"
    --source "${CMAKE_SOURCE_DIR}" --build "${CMAKE_BINARY_DIR}"
    --defconfig "${KCONFIG_DEFCONFIG}")
execute_process(COMMAND ${kconfig_command} RESULT_VARIABLE kconfig_result)
if(NOT kconfig_result EQUAL 0)
    message(FATAL_ERROR "Kconfig generation failed; see diagnostics above")
endif()
include("${CMAKE_BINARY_DIR}/generated/config.cmake")
# Regenerate before compilation when saved options, schemas, or generator change.
file(GLOB_RECURSE kconfig_inputs CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/bsp/*/Kconfig"
    "${CMAKE_SOURCE_DIR}/modules/*/Kconfig"
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Kconfig" ${kconfig_inputs}
    "${CMAKE_SOURCE_DIR}/scripts/configure.py" "${KCONFIG_DEFCONFIG}"
    "${CMAKE_BINARY_DIR}/.config"
    "${CMAKE_BINARY_DIR}/generated/build_config.h"
    "${CMAKE_BINARY_DIR}/generated/autoconf.h"
)
add_custom_target(menuconfig COMMAND ${kconfig_command} --menuconfig
    USES_TERMINAL VERBATIM COMMENT "Edit firmware configuration")
add_custom_target(savedefconfig COMMAND ${kconfig_command} --savedefconfig
    VERBATIM COMMENT "Save minimal configuration to the build directory")
