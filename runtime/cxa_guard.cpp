#include <cstdint>

#include <bits.h>
#include <cpu_port.h>

namespace {

    constexpr auto initialized = bit<std::uint32_t, 0>;
    constexpr auto in_progress = bit<std::uint32_t, 1>;

    extern "C" int __cxa_guard_acquire(volatile std::uint32_t* guard)
    {
        const auto irq_state = cpu::port::irq_save_disable();
        const auto state = *guard;
        if (state & initialized)
        {
            cpu::port::irq_restore(irq_state);
            return 0;
        }

        if (!(state & in_progress))
        {
            *guard = in_progress;
            cpu::port::data_memory_barrier();
            cpu::port::irq_restore(irq_state);
            return 1;
        }

        cpu::port::fatal_trap();
        return -1;
    }

    extern "C" void __cxa_guard_release(volatile std::uint32_t* guard)
    {
        cpu::port::data_memory_barrier();
        *guard = initialized;
        cpu::port::data_memory_barrier();
    }

    extern "C" void __cxa_guard_abort(volatile std::uint32_t* guard)
    {
        *guard = 0;
        cpu::port::data_memory_barrier();
    }

}