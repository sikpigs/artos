#pragma once

#include <build_config.h>

#include <internal/thread_queue.h>

#include <array>

namespace artos::kernel::internal {

    extern std::array<thread_queue_t, build_config::artos::priority_levels> ready_queue;
    extern thread_queue_t                                                   block_sleep_queue;

    extern std::array<thread::internal::tcb_base_t*, build_config::artos::max_threads> threads;
    extern std::array<std::uint32_t, 8>                                                ready_bitmap;

    extern std::size_t                   thread_count;
    extern thread::internal::tcb_base_t* current_thread;
    extern thread::internal::tcb_base_t* next_thread;

    extern std::size_t   runtime_history_bucket_index;
    extern std::size_t   runtime_history_window_start;
    extern std::uint32_t runtime_history_window_counter;
    extern std::uint32_t runtime_history_bucket_cycles;
}
