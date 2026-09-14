#include <cpu_port.h>

namespace cpu::port {
    primask_t irq_save_disable() noexcept
    {
        primask_t state;
        __asm volatile(
            "mrs %0, primask\n"
            "cpsid i"
            : "=r"(state)
            :
            : "memory");
        return state;
    }

    void irq_restore(primask_t state) noexcept
    {
        __asm volatile("msr primask, %0" : : "r"(state) : "memory");
    }

    void data_memory_barrier() noexcept
    {
        __asm volatile("dmb" ::: "memory");
    }

    void wait_for_interrupt() noexcept
    {
        __asm volatile("dsb\nwfi" ::: "memory");
    }

    [[nodiscard]] basepri_t get_basepri() noexcept
    {
        basepri_t state;
        __asm volatile("mrs %0, basepri" : "=r"(state) : : "memory");
        return state;
    }

    void set_basepri(basepri_t state) noexcept
    {
        __asm volatile("msr basepri, %0" : : "r"(state) : "memory");
    }

    void raise_basepri(basepri_t state) noexcept
    {
        __asm volatile("msr basepri_max, %0" : : "r"(state) : "memory");
    }

    [[nodiscard]] basepri_t irq_save_mask_below(basepri_t priority) noexcept
    {
        basepri_t state;
        __asm volatile(
            "mrs %0, basepri\n"
            "msr basepri_max, %1"
            : "=&r"(state)
            : "r"(priority)
            : "memory");
        return state;
    }

    void irq_restore_mask(basepri_t state) noexcept
    {
        __asm volatile("msr basepri, %0" : : "r"(state) : "memory");
    }

    [[nodiscard]] bool is_in_irq() noexcept
    {
        std::uint32_t current_int;
        __asm volatile("mrs %0, ipsr" : "=r"(current_int) : : "memory");
        return current_int != 0;
    }

    [[noreturn]] void fatal_trap() noexcept
    {
        (void)irq_save_disable();
        auto const* const       dhcsr = reinterpret_cast<volatile const std::uint32_t*>(0xE000EDF0UL);
        constexpr std::uint32_t c_debugen = 1U;
        if ((*dhcsr & c_debugen) != 0U)
        {
            __asm volatile("bkpt #0" ::: "memory");
        }
        for (;;)
        {
        }
    }
}
