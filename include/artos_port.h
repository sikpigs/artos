#pragma once

// Selected CPU port. A future target selector can choose a different traits header.
#include <ports/cortex_m4.h>

namespace artos::port {
    void          start_tick() noexcept;
    stack_word_t* initialize_stack(stack_word_t* stack_top, void (*entry)(void*), void* argument) noexcept;
}
