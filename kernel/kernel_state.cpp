#include <build_config.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>

using namespace build_config::artos;

namespace artos::kernel::internal {
    using namespace thread::internal;

    std::array<tcb_base_t*, build_config::artos::max_threads> threads {};

    std::size_t thread_count = 0;
    tcb_base_t* current_thread = nullptr;
    tcb_base_t* next_thread = nullptr;

    std::array<thread_queue_t, priority_levels> ready_queue {};
    thread_queue_t                              block_sleep_queue {};
    std::array<std::uint32_t, 8>                ready_bitmap {};

    std::size_t   runtime_history_bucket_index {};
    std::size_t   runtime_history_window_start {};
    std::uint32_t runtime_history_window_counter {};
    std::uint32_t runtime_history_bucket_cycles {};
}
