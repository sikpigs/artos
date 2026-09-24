#include <build_config.h>

#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/thread_state.h>

using namespace build_config::artos;

namespace artos::thread::internal {
    std::array<tcb_base_t*, build_config::artos::max_threads> threads {};

    std::size_t thread_count = 0;
    tcb_base_t* current_thread = nullptr;
    tcb_base_t* next_thread = nullptr;

    std::array<thread_queue_t, priority_levels> ready_queue {};
    thread_queue_t                              block_sleep_queue {};
    std::array<std::uint32_t, 8>                ready_bitmap {};
}
