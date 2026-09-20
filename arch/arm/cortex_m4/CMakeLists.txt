target_sources(firmware PRIVATE
    src/cpu.cpp
    src/reset.s
    src/fault_handlers.s
    src/fault_capture.cpp
    src/irq.cpp
    include/arch/artos.h
    include/arch/regmap.h
)

# Fault capture must not disturb the floating-point state being diagnosed.
set_source_files_properties(src/fault_capture.cpp
    DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTIES COMPILE_OPTIONS "-mgeneral-regs-only")

if(CONFIG_ARTOS)
    target_sources(firmware PRIVATE
        src/artos.cpp
        src/pendsv.s
        src/svc.s
    )
endif()
