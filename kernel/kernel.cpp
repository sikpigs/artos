
#include <artos.h>
#include <artos_port.h>
#include <atomic>

#include "internal/deferred.h"
#include "internal/scheduler.h"

namespace artos::kernel {
    namespace {
        // Slot zero is reserved by init(). This stack is for the idle loop;
        // exception handlers execute on MSP. Revisit its budget as the port grows.
        thread::thread_storage<thread::minimum_stack_bytes> idle_storage {};

        [[noreturn]] void idle_entry(void*) noexcept
        {
            for (;;)
            {
                cpu::port::wait_for_interrupt();
            }
        }
    }

    namespace internal {
        std::atomic<tick_t> ticks { 0 };
        static_assert(std::atomic<tick_t>::is_always_lock_free);

        void on_tick() noexcept
        {
            ticks.fetch_add(1, std::memory_order_relaxed);
            deferred::internal::serve_deferred();
            sched();
        }
    }

    void init() noexcept
    {
        const critical_section_t cs {};
        thread::internal::threads.fill(nullptr);
        thread::internal::thread_count = 0;
        thread::internal::current_thread = nullptr;
        thread::internal::next_thread = nullptr;
        internal::ticks.store(0, std::memory_order_relaxed);
        // Idle is the fallback, even when an application uses priority_t::low.
        // Future selection must consider ready application threads first.
        const auto idle = thread::create(idle_entry, "idle", nullptr, thread::priority_t::low, idle_storage);
        if (!idle || *idle != 0)
        {
            cpu::port::fatal_trap();
        }

        port::init();
        port::start_tick();
    }

    extern "C" [[noreturn]] void start_first_task();

    [[noreturn]]
    void start() noexcept
    {
        thread::internal::next_thread = internal::select_next();
        start_first_task();
    }

    [[nodiscard]]
    tick_t ticks() noexcept
    {
        return internal::ticks.load(std::memory_order_relaxed);
    }
}
