#pragma once

#include <cstdint>

// Always available, including builds without ARTOS. The current token is the
// Cortex-M PRIMASK value; callers must preserve it unchanged.
namespace cpu::port {
    using primask_t = std::uint32_t;
    using basepri_t = std::uint32_t;

    [[nodiscard]] primask_t get_primask() noexcept;
    void                    set_primask(primask_t state) noexcept;

    [[nodiscard]] basepri_t get_basepri() noexcept;
    void                    set_basepri(basepri_t state) noexcept;
    void                    raise_basepri(basepri_t state) noexcept;

    [[nodiscard]] primask_t irq_save_disable() noexcept;
    void                    irq_restore(primask_t state) noexcept;

    [[nodiscard]] basepri_t irq_save_mask_below(basepri_t priority) noexcept;
    void                    irq_restore_mask(basepri_t priority) noexcept;

    [[nodiscard]] bool is_in_irq() noexcept;

    // Orders memory accesses in both the compiler and the processor.
    void data_memory_barrier() noexcept;

    // Wait for an interrupt; callers must recheck their work on return.
    void wait_for_interrupt() noexcept;

    [[noreturn]] void fatal_trap() noexcept;
}
