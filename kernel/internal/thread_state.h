#pragma once

#include <build_config.h>

#include <internal/thread.h>
#include <internal/thread_queue.h>

#include <array>

namespace artos::thread::internal {
    extern std::array<thread_queue_t, build_config::artos::priority_levels> ready_queue;
    extern thread_queue_t                                                   block_sleep_queue;

    extern std::array<tcb_t*, build_config::artos::max_threads> threads;
    extern std::array<std::uint32_t, 8>                         ready_bitmap;

    extern std::size_t thread_count;
    extern tcb_t*      current_thread;
    extern tcb_t*      next_thread;
}
