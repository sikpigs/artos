#include <build_config.h>

#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/thread_state.h>
#include <internal/time.h>

#include <algorithm>
#include <cerrno>
#include <new>

#include <artos.h>
#include <artos_port.h>
#include <wait_queue.h>

namespace artos::thread {
    namespace internal {
        start_result_t create(func_t              func,
                              std::string_view    name,
                              std::uint32_t       stack_size_bytes,
                              void*               parameters,
                              priority_t          priority,
                              std::byte* const    tcb,
                              stack_word_t* const stack,
                              bool                start) noexcept
        {
            const critical_section_t cs {};

            auto* thread = reinterpret_cast<tcb_base_t*>(tcb);
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

            thread = ::new (static_cast<void*>(tcb)) tcb_t<decltype(build_config::artos::scheduler)> {};
            const auto stack_word_count = stack_size_bytes / sizeof(stack_word_t);
            auto*      sp = port::initialize_stack(stack + stack_word_count, func, parameters);

            thread->sp = sp;
            thread->stack_begin = stack;
            thread->stack_end = stack + stack_word_count;

            thread->name = name;
            thread->priority = priority;
            thread->deadline = {};

            ++thread_count;
            *tcb_it = thread;

            if (start)
            {
                make_thread_ready(thread);
            }
            else
            {
                thread->state = thread_state_t::suspended;
            }

            const auto index = std::distance(threads.begin(), tcb_it);
            return thread->make_thread_id(static_cast<std::size_t>(index));
        }

        void sleep_until(const tick_time_t& tick_time)
        {
            if (current_thread == nullptr)
            {
                return;
            }

            const artos::critical_section_t cs {};

            if (kernel::internal::deadline_reached(kernel::tick_time(), tick_time))
            {
                return;
            }

            current_thread->deadline = tick_time;
            current_thread->state = thread_state_t::sleeping;

            block_sleep_thread(current_thread);

            // Select and publish a runnable destination before requesting PendSV.
            // The switch executes after this critical section restores interrupts.
            kernel::internal::sched();
        }
    }

    id_t get_current_thread() noexcept
    {
        return internal::current_thread ? internal::current_thread->id : invalid_id;
    }

    void sleep_for(tick_t ticks) noexcept
    {
        if (internal::current_thread == nullptr || ticks == 0)
        {
            return;
        }

        const auto now = kernel::tick_time();
        internal::sleep_until(now + ticks);
    }

    void sleep_until(const tick_time_t& tick_time) noexcept
    {
        internal::sleep_until(tick_time);
    }
}
