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

        struct atomic_tick_time_t
        {
            atomic_tick_time_t() noexcept = default;

            atomic_tick_time_t(tick_t tick, tick_t gen) noexcept
                : ticks { tick }
                , generation { gen }
            {
            }

            ~atomic_tick_time_t() noexcept = default;

            void reset() noexcept
            {
                ticks.exchange(0, std::memory_order_relaxed);
                generation.exchange(0, std::memory_order_relaxed);
            }

            atomic_tick_time_t& operator=(const atomic_tick_time_t& other) noexcept
            {
                if (std::addressof(other) != this)
                {
                    ticks.store(other.ticks.load(std::memory_order_relaxed));
                    generation.store(other.generation.load(std::memory_order_relaxed));
                }

                return *this;
            }

            atomic_tick_time_t& operator++() noexcept
            {
                ticks.fetch_add(1, std::memory_order_relaxed);

                const auto now = ticks.load(std::memory_order_relaxed);

                if (now == 0)
                {
                    generation.fetch_add(1, std::memory_order_relaxed);
                }

                return *this;
            }

            atomic_tick_time_t operator++(int) noexcept
            {
                const auto prev_ticks = ticks.fetch_add(1, std::memory_order_relaxed);
                const auto prev_gen = generation.load(std::memory_order_relaxed);

                const auto now = ticks.load(std::memory_order_relaxed);

                if (now == 0)
                {
                    generation.fetch_add(1, std::memory_order_relaxed);
                }

                return { prev_ticks, prev_gen };
            }

            operator tick_time_t() const noexcept
            {
                return { .ticks = ticks.load(std::memory_order_relaxed),
                         .generation = generation.load(std::memory_order_relaxed) };
            }

        private:
            std::atomic<tick_t> ticks { 0 };
            std::atomic<tick_t> generation { 0 };
            static_assert(std::atomic<tick_t>::is_always_lock_free);
        };

        atomic_tick_time_t ticks {};

        void on_tick() noexcept
        {
            ++ticks;
            deferred::internal::serve_deferred();
            sched();
        }
    }

    namespace locked {
        [[nodiscard]]
        tick_time_t tick_time() noexcept
        {
            return static_cast<tick_time_t>(internal::ticks);
        }
    }

    void init() noexcept
    {
        const global_interrupt_guard_t cs {};
        thread::internal::threads.fill(nullptr);
        thread::internal::thread_count = 0;
        thread::internal::current_thread = nullptr;
        thread::internal::next_thread = nullptr;

        internal::ticks.reset();

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
        return tick_time().ticks;
    }

    [[nodiscard]]
    tick_time_t tick_time() noexcept
    {
        const critical_section_t cs {};
        return static_cast<tick_time_t>(internal::ticks);
    }
}
