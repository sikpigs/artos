#pragma once

#include <internal/thread.h>

namespace artos::kernel::internal {
    thread::internal::tcb_base_t* select_next() noexcept;

    void sched() noexcept;
}
