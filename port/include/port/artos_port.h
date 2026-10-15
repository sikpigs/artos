#pragma once

// Selected CPU port. A future target selector can choose a different traits header.
#include <arch/artos.h>

#include <port/cpu_port.h>

namespace artos::port {
    // Preserve the kernel-facing names; implementation belongs to CPU support.
    using cpu::port::primask_t;
    using cpu::port::irq_save_disable;
    using cpu::port::irq_restore;

    inline constexpr cpu::port::basepri_t kernel_interrupt_priority = 0xF0;

    struct stack_bounds_t
    {
        std::byte* bottom;
        std::byte* top;
    };

    enum class validate_stack_protection_result_t : std::uint8_t
    {
        success,
        invalid
    };

    validate_stack_protection_result_t validate_state_protection(stack_bounds_t) noexcept;
    void                               activate_stack_protection(stack_bounds_t) noexcept;

    // Configure kernel exceptions before enabling the tick.
    void init() noexcept;
    void start_tick() noexcept;

    stack_word_t* initialize_stack(stack_word_t* stack_top, void (*entry)(void*), void* argument) noexcept;

    std::uint32_t get_cycles_remaining() noexcept;

    void pendsvset() noexcept;
}
