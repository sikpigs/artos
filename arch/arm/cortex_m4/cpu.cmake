# Selected by the SoC through Kconfig; the toolchain only locates tools.
set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

add_compile_options(${MCU_FLAGS})
add_link_options(${MCU_FLAGS})

