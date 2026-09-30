#include <board_devices.h>
#include <board_services.h>
#include <build_config.h>

#include <cpu_port.h>
#include <internal/deferred.h>
#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>

#include <atomic>
#include <limits>
#include <memory>

#ifdef CONFIG_CONSOLE
#include <console/console.h>
#endif

#include <artos.h>
#include <artos_port.h>

namespace artos::kernel {
    using namespace build_config::artos;
    using namespace kernel::internal;

    namespace {
        constexpr auto runtime_history_bucket_cycles_config
            = (static_cast<std::uint64_t>(board::devices::stopwatch_clock_hz.count()) * runtime_history_bucket_ms)
              / 1000;
        constexpr auto runtime_history_tick_cycles_config
            = (static_cast<std::uint64_t>(board::devices::stopwatch_clock_hz.count()) + tick_hz - 1) / tick_hz;
        static_assert(!thread_stats
                          || (runtime_history_bucket_cycles_config > 0
                              && runtime_history_bucket_cycles_config + runtime_history_tick_cycles_config
                                     <= std::numeric_limits<std::uint32_t>::max()),
                      "runtime history bucket plus one tick exceeds 32-bit cycle capacity");

        // Slot zero is reserved by init(). This stack is for the idle loop;
        // exception handlers execute on MSP. Revisit its budget as the port grows.
        thread::storage_t<thread::minimum_stack_bytes> idle_storage {};

        [[noreturn]] void idle_entry(void*) noexcept
        {
            using namespace thread::internal;
            using namespace internal;

            for (;;)
            {
                {
                    const critical_section_t cs {};
                    for (std::size_t ix = 0; ix < threads.size(); ++ix)
                    {
                        if (threads[ix] == nullptr || threads[ix]->state != thread::state_t::finished)
                        {
                            continue;
                        }

                        if (threads[ix]->on_exit)
                        {
                            threads[ix]->on_exit(threads[ix]->on_exit_ctx);
                        }

                        threads[ix] = nullptr;

                        --thread_count;
                    }
                }

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

        void collect_stats() noexcept
        {
            if constexpr (thread_stats)
            {
                const auto now = board::stopwatch::count();
                const auto delta = now - static_cast<std::uint32_t>(runtime_history_window_start);
                runtime_history_window_start = now;
                runtime_history_window_counter += delta;

                if (current_thread)
                {
                    reinterpret_cast<thread::internal::tcb_t*>(current_thread)->keep_running(now);
                }

                while (runtime_history_window_counter >= runtime_history_bucket_cycles)
                {
                    runtime_history_bucket_index = (runtime_history_bucket_index + 1) % runtime_history_bucket_count;
                    runtime_history_window_counter -= runtime_history_bucket_cycles;

                    for (auto* thread : threads)
                    {
                        if (thread != nullptr)
                        {
                            reinterpret_cast<thread::internal::tcb_t*>(thread)->clear_runtime_history_bucket(
                                runtime_history_bucket_index);
                        }
                    }
                }
            }
        }

        void on_tick() noexcept
        {
            ++ticks;
            deferred::internal::serve_deferred();
            sched();
            collect_stats();
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
        threads.fill(nullptr);
        thread_count = 0;
        current_thread = nullptr;
        next_thread = nullptr;

        internal::ticks.reset();

        // Idle is the fallback, even when an application uses priority_t::low.
        // Future selection must consider ready application threads first.
        const auto idle = thread::create(idle_entry, "IDLE", nullptr, thread::priority_t::low, idle_storage, false);
        if (!idle || *idle != 0)
        {
            cpu::port::fatal_trap();
        }

        port::init();
        port::start_tick();

        if constexpr (build_config::artos::thread_stats)
        {
            board::stopwatch::init();
            runtime_history_bucket_index = 0;
            runtime_history_window_counter = 0;
            runtime_history_window_start = board::stopwatch::count();
            runtime_history_bucket_cycles = static_cast<std::uint32_t>(runtime_history_bucket_cycles_config);
        }
    }

    extern "C" [[noreturn]] void start_first_task();

    [[noreturn]]
    void start() noexcept
    {
        next_thread = select_next();
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

    extern "C" void app_main() noexcept;

    void main(void*) noexcept
    {
        app_main();
    }

    extern "C" void kmain() noexcept
    {
        board::init();

#ifdef CONFIG_CONSOLE
        artos::console::system_console().init();
#endif

        init();

        [[maybe_unused]] const auto main_thread = thread::create<main_thread_stack_size>(
            main, "MAIN", nullptr, thread::make_priority<main_thread_priority>(), true);

        start();
    }
}
