#pragma once

#include "thread.h"

#include <array>
#include <cstddef>

// Private declarations for the existing shared state. Defined in scheduler.cpp.
namespace artos::thread::internal {
    using ready_queue_t = intrusive_list_t;

    extern std::array<ready_queue_t, build_config::artos::priority_levels> ready_queue;
    extern std::array<tcb_t*, build_config::artos::max_threads>            threads;
    extern std::size_t                                                     thread_count;
    extern tcb_t*                                                          current_thread;
    extern tcb_t*                                                          next_thread;
}

namespace artos::kernel::internal {
    thread::internal::tcb_t* select_next() noexcept;
    void                     sched() noexcept;
}
