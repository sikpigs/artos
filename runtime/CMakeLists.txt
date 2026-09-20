# Freestanding C/C++ runtime hooks are required even without the scheduler.
target_sources(firmware PRIVATE
    init.cpp
    abort.cpp
    cxa_guard.cpp
    syscalls.cpp
)
