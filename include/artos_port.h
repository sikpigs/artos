#pragma once

// Selected CPU port. A future target selector can choose a different traits header.
#include <arch/artos.h>
#include <cpu_port.h>

namespace artos::port {
    // Preserve the kernel-facing names; implementation belongs to CPU support.
    using cpu::port::primask_t;
    using cpu::port::irq_save_disable;
    using cpu::port::irq_restore;

    // Configure kernel exceptions before enabling the tick.
    void init() noexcept;
    void start_tick() noexcept;

    stack_word_t* initialize_stack(stack_word_t* stack_top, void (*entry)(void*), void* argument) noexcept;

    void pendsvset() noexcept;
}
