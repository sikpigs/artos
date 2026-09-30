#include <build_config.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/time.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <new>

#include <artos.h>
#include <artos_port.h>
#include <cpu_port.h>
#include <wait_queue.h>

using namespace build_config::artos;

namespace artos::thread {
    using namespace build_config::artos;
    using namespace internal;
    using namespace kernel;
    using namespace kernel::internal;

    namespace internal {
        start_result_t create(func_t              func,
                              std::string_view    name,
                              std::uint32_t       stack_size_bytes,
                              void*               parameters,
                              priority_t          priority,
                              std::byte* const    tcb,
                              stack_word_t* const canary,
                              stack_word_t* const stack,
                              bool                start) noexcept
        {
            const critical_section_t cs {};

            if (name.size() > max_thread_name_length)
            {
                return artos::unexpected(ENAMETOOLONG);
            }

            auto* thread = reinterpret_cast<tcb_t*>(tcb);
            if (std::ranges::find(threads, thread) != threads.end())
            {
                return artos::unexpected(EEXIST);
            }
            if (thread_count == threads.size())
            {
                return artos::unexpected(EAGAIN);
            }

            auto tcb_it = std::ranges::find(threads, nullptr);
            if (tcb_it == threads.end())
            {
                return artos::unexpected(EAGAIN);
            }

            const auto stack_word_count = stack_size_bytes / sizeof(stack_word_t);
            if constexpr (tcb_t::high_watermark_enabled)
            {
                std::fill_n(stack, stack_word_count, stack_fill_pattern);
            }

            if constexpr (thread_stack_canary)
            {
                std::fill_n(canary, canary_word_count, stack_fill_pattern);
            }

            auto* sp = port::initialize_stack(stack + stack_word_count, func, parameters);

            thread = ::new (static_cast<void*>(tcb)) tcb_t { name, priority, sp, stack, stack_word_count, canary };

            ++thread_count;
            *tcb_it = thread;

            if (start)
            {
                make_thread_ready(thread);
            }
            else
            {
                thread->state = thread::state_t::suspended;
            }

            const auto index = std::distance(threads.begin(), tcb_it);
            return thread->make_thread_id(static_cast<std::size_t>(index));
        }

        void sleep_until(const tick_time_t& deadline)
        {
            if (current_thread == nullptr)
            {
                return;
            }

            const artos::critical_section_t cs {};

            if (deadline_reached(tick_time(), deadline))
            {
                return;
            }

            current_thread->deadline = deadline;
            current_thread->state = state_t::sleeping;

            block_sleep_thread(current_thread);

            // Select and publish a runnable destination before requesting PendSV.
            // The switch executes after this critical section restores interrupts.
            sched();
        }

        void on_exit(id_t thread_id, func_t on_exit, void* ctx) noexcept
        {
            const auto index = tcb_t::id_layout::extract_id(thread_id);

            assert(index < threads.size());

            auto* thread = threads[index];

            assert(thread->id == thread_id);

            thread->on_exit = on_exit;
            thread->on_exit_ctx = ctx;
        }
    }

    id_t get_current_thread() noexcept
    {
        return current_thread ? current_thread->id : invalid_id;
    }

    void sleep_for(tick_t ticks) noexcept
    {
        if (current_thread == nullptr || ticks == 0)
        {
            return;
        }

        const auto now = tick_time();
        internal::sleep_until(now + ticks);
    }

    void sleep_until(const tick_time_t& tick_time) noexcept
    {
        internal::sleep_until(tick_time);
    }

    extern "C" [[noreturn]] void thread_exit() noexcept
    {
        assert(current_thread);

        auto* thread = current_thread;
        thread->state = thread::state_t::finished;
        sched();

        for (;;)
        {
            cpu::port::wait_for_interrupt();
        }
    }

    [[noreturn]] void exit() noexcept
    {
        thread_exit();
    }

    [[nodiscard]] std::size_t get_high_watermark() noexcept
    {
        if constexpr (tcb_t::high_watermark_enabled)
        {
            return reinterpret_cast<tcb_t*>(current_thread)->get_high_watermark();
        }
        else
        {
            return 0;
        }
    }

    void get_statistics(statistics_t& statistics) noexcept
    {
        if constexpr (internal::tcb_t::statistics_enabled)
        {
            reinterpret_cast<tcb_t*>(current_thread)->get_statistics(statistics);
        }
    }

    std::size_t get_snapshot(snapshot_array_t& snapshot) noexcept
    {
        const critical_section_t cs {};

        std::size_t count = 0;
        for (auto* base : threads)
        {
            if (base == nullptr)
            {
                continue;
            }

            auto* thread = static_cast<tcb_t*>(base);
            auto& entry = snapshot[count++];
            entry.id = thread->id;
            entry.name = thread->name;
            entry.priority = thread->priority;
            entry.state = thread->state;
            entry.statistics = {};
            thread->get_statistics(entry.statistics);
        }

        return count;
    }
}
