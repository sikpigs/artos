# Freestanding C/C++ runtime hooks are required even without the scheduler.
target_sources(${FIRMWARE_TARGET} PRIVATE
    init.cpp
    abort.cpp
    cxa_guard.cpp
    syscalls.cpp
)
