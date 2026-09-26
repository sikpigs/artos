#include <build_config.h>

#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/thread_queue.h>
#include <internal/thread_state.h>
#include <internal/time.h>

#include <cassert>
#include <utility>

#include <artos.h>
#include <artos_port.h>
#include <wait_queue.h>

using namespace build_config::artos;

namespace artos::kernel::internal {
    using namespace thread::internal;

    void check_deadlines() noexcept
    {
        const auto now = kernel::tick_time();

        auto it = block_sleep_queue.begin();
        while (it != block_sleep_queue.end())
        {
            auto* thread = it->thread;

            if ((thread->state == thread_state_t::sleeping || thread->state == thread_state_t::blocked))
            {
                if (deadline_reached(now, thread->deadline))
                {
                    if (thread->state == thread_state_t::blocked)
                    {
                        thread->waiter.queue->remove(thread->waiter);
                        thread->waiter.wake = wake_reason_t::timeout;
                    }

                    make_thread_ready(thread);

                    auto& node = *it;
                    ++it;
                    block_sleep_queue.remove(node);
                    continue;
                }
                else
                {
                    break;
                }
            }

            ++it;
        }
    }

    [[nodiscard]]
    tcb_t* select_next(fixed_priority_round_robin_config) noexcept
    {
        if (current_thread != nullptr && current_thread != threads[0])
        {
            if (current_thread->state == thread_state_t::running || current_thread->state == thread_state_t::ready)
            {
                current_thread->ready.queue->rotate_after(current_thread->ready);
            }
            else if (current_thread->ready.queue != nullptr)
            {
                auto* queue = current_thread->ready.queue;
                queue->remove(current_thread->ready);
                if (queue->empty())
                {
                    clear_priority_ready(current_thread->priority);
                }
            }
        }

        auto priority = get_highest_set_priority();
        if (!priority)
        {
            return threads[0];
        }

        auto& queue = ready_queue[std::to_underlying(*priority)];
        return reinterpret_cast<tcb_t*>(queue.front()->thread);
    }

    tcb_t* select_next() noexcept
    {
        return select_next(build_config::artos::scheduler);
    }

    template<bool aging_enabled>
    struct ager;

    template<>
    struct ager<false>
    {
        void operator()() const noexcept
        {
        }
    };

    template<>
    struct ager<true>
    {
        void operator()() const noexcept
        {
            for (std::size_t ix = 1; ix < threads.size(); ++ix)
            {
                if (threads[ix] == nullptr || threads[ix] == next_thread || threads[ix] == current_thread)
                {
                    continue;
                }

                threads[ix]->age();
            }
        }
    };

    inline static constexpr ager<scheduler.priority_aging_ticks.has_value()> age {};

    void sched() noexcept
    {
        // Leave the PendSV startup selection intact until a thread is running.
        if (current_thread == nullptr)
        {
            return;
        }

        check_deadlines();

        auto* selected = select_next(scheduler);
        // Refresh even if a previously requested PendSV is already pending
        // and a new tick now selects the current thread again.
        next_thread = selected;

        age();

        next_thread->reset_priority();

        if (selected != current_thread)
        {
            artos::port::pendsvset();
        }
    }
}
