#include <cstdint>

#include <asm.h>
#include <bits.h>

namespace {

    constexpr auto initialized = bit<std::uint32_t, 0>;
    constexpr auto in_progress = bit<std::uint32_t, 1>;
    constexpr auto interrupts_were_masked = bit<std::uint32_t, 2>;

    [[nodiscard]] std::uint32_t mask_interrupts()
    {
        std::uint32_t primask;
        __asm volatile(
            "mrs %0, primask\n"
            "cpsid i"
            : "=r"(primask)
            :
            : "memory");
        return primask;
    }

    void restore_interrupts(std::uint32_t primask)
    {
        if (!(primask & 1))
        {
            __asm volatile("cpsie i" ::: "memory");
        }
    }

    [[noreturn]]
    void recursive_initialization()
    {
        __asm volatile("bkpt #0");
        for (;;)
        {
        }
    }

    extern "C" int __cxa_guard_acquire(volatile std::uint32_t* guard)
    {
        const auto primask = mask_interrupts();
        const auto state = *guard;
        if (state & initialized)
        {
            restore_interrupts(primask);
            return 0;
        }

        if (state & in_progress)
        {
            recursive_initialization();
        }

        *guard = in_progress | (primask & 1 ? interrupts_were_masked : 0);
        dmb();
        return 1;
    }

    extern "C" void __cxa_guard_release(volatile std::uint32_t* guard)
    {
        const auto state = *guard;
        dmb();
        *guard = initialized;
        dmb();
        restore_interrupts((state & interrupts_were_masked) ? 1 : 0);
    }

    extern "C" void __cxa_guard_abort(volatile std::uint32_t* guard)
    {
        const auto state = *guard;
        *guard = 0;
        dmb();
        restore_interrupts((state & interrupts_were_masked) ? 1 : 0);
    }

}