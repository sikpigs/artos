#include <build_config.h>

#include <cstddef>
#include <defs.h>
#include <internal/container_of.h>
#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/time.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <intrusive_list.h>
#include <new>

#include <artos.h>
#include <artos_port.h>
#include <cpu_port.h>
#include <optional>
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

    void finish_thread(tcb_base_t* thread) noexcept
    {
        if (thread->state == thread::state_t::finished)
        {
            return;
        }

        thread->state = thread::state_t::finished;
        locked::wake_all(thread->joiners);
        sched();
    }

    [[noreturn]] void exit() noexcept
    {
        assert(current_thread);

        const critical_section_t cs {};
        finish_thread(current_thread);

        for (;;)
        {
            cpu::port::wait_for_interrupt();
        }
    }

    extern "C" [[noreturn]] void thread_exit() noexcept
    {
        exit();
    }

    kill_result_t kill(id_t id) noexcept
    {
        const auto ix = tcb_t::id_layout::extract_id(id);
        if (ix >= threads.size() || ix == 0)
        {
            return kill_result_t::invalid_thread;
        }

        const critical_section_t cs {};

        auto* thread = threads[ix];
        if (thread == nullptr || thread->id != id)
        {
            return kill_result_t::invalid_thread;
        }

        if (thread == current_thread)
        {
            finish_thread(thread);
        }
        else
        {
            if (thread->ready.queue)
            {
                auto* queue = thread->ready.queue;
                queue->remove(thread->ready);
                if (queue->empty())
                {
                    clear_priority_ready(thread->priority);
                }
            }

            if (thread->waiter.queue)
            {
                auto* queue = thread->waiter.queue;
                queue->remove(thread->waiter);
                thread->waiter.wake = wake_reason_t::cancelled;
                if (thread->waiting_mutex)
                {
                    prioritize(thread->waiting_mutex);
                    thread->waiting_mutex = nullptr;
                }
            }

            if (thread->block_sleep.queue)
            {
                thread->block_sleep.queue->remove(thread->block_sleep);
            }

            thread->state = thread::state_t::finished;
            locked::wake_all(thread->joiners);
            sched();
        }

        return kill_result_t::success;
    }

    join_result_t join(id_t id, std::optional<tick_t> ticks_to_wait) noexcept
    {
        const auto ix = tcb_t::id_layout::extract_id(id);
        if (ix >= threads.size())
        {
            return join_result_t::invalid_thread;
        }

        critical_section_t cs {};

        auto* thread = threads[ix];
        if (thread == nullptr || thread->id != id)
        {
            return join_result_t::invalid_thread;
        }

        if (current_thread == nullptr)
        {
            return join_result_t::deadlock_detected;
        }

        if (thread->state == thread::state_t::finished)
        {
            return join_result_t::success;
        }

        if (thread == current_thread)
        {
            return join_result_t::deadlock_detected;
        }

        intrusive_list_t stack {};
        stack.push_front(current_thread->join_hook);

        while (!stack.empty())
        {
            auto* hook = stack.pop_front();
            auto* waiting_thread = ARTOS_CONTAINER_OF(hook, tcb_base_t, join_hook);
            for (auto it = waiting_thread->joiners.begin(); it != waiting_thread->joiners.end(); ++it)
            {
                if (it->thread == thread)
                {
                    while (!stack.empty())
                    {
                        (void)stack.pop_front();
                    }
                    return join_result_t::deadlock_detected;
                }

                stack.push_front(it->hook);
            }
        }

        const auto deadline = kernel::tick_time() + ticks_to_wait.value_or(0);

        wake_reason_t reason;
        if (ticks_to_wait)
        {
            reason = locked::block_current(thread->joiners, deadline, std::move(cs));
        }
        else
        {
            reason = locked::block_current(thread->joiners, wait_forever, std::move(cs));
        }

        return reason == wake_reason_t::timeout ? join_result_t::timeout : join_result_t::success;
    }

    join_result_t join(id_t id, tick_t ticks_to_wait) noexcept
    {
        return join(id, std::make_optional(ticks_to_wait));
    }

    join_result_t join(id_t id, wait_forever_t) noexcept
    {
        return join(id, std::nullopt);
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
