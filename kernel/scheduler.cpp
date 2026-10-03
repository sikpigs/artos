#include <cassert>
#include <utility>

#include <build_config.h>

#include <artos.h>
#include <artos_port.h>
#include <mutex.h>
#include <wait_queue.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/thread_queue.h>
#include <internal/time.h>

using namespace build_config::artos;

namespace artos::kernel::internal {
    using namespace thread::internal;
    using namespace kernel::internal;

    void prioritize(mutex_t* mutex) noexcept
    {
        mutex->prioritize();
    }

    template<typename Thread>
    thread::priority_t include_aging(Thread* owning_thread, thread::priority_t effective) noexcept
    {
        if constexpr (Thread::priority_aging_enabled)
        {
            if (owning_thread->aged_priority)
            {
                effective = std::min(effective, *owning_thread->aged_priority);
            }
        }

        return effective;
    }

    void recalculate_priority(thread::id_t thread_id) noexcept
    {
        auto ix = tcb_base_t::id_layout::extract_id(thread_id);
        assert(ix < threads.size());
        auto* thread = reinterpret_cast<tcb_t*>(threads[ix]);
        assert(thread == nullptr || thread->id == thread_id);

        auto effective = include_aging(thread, thread->base_priority);

        for (auto owned_mutex = thread->mutexes.begin(); owned_mutex != thread->mutexes.end(); ++owned_mutex)
        {
            for (auto waiter : owned_mutex->waiters)
            {
                auto* waiting_thread = waiter.thread;
                effective = std::min(effective, waiting_thread->priority);
            }
        }

        set_effective_priority(thread, effective);
    }

    void check_deadlines() noexcept
    {
        const auto now = kernel::tick_time();

        auto it = block_sleep_queue.begin();
        while (it != block_sleep_queue.end())
        {
            auto* thread = it->thread;

            if ((thread->state == thread::state_t::sleeping || thread->state == thread::state_t::blocked))
            {
                if (deadline_reached(now, thread->deadline))
                {
                    if (thread->state == thread::state_t::blocked)
                    {
                        thread->waiter.queue->remove(thread->waiter);
                        thread->waiter.wake = wake_reason_t::timeout;

                        if (thread->waiting_mutex)
                        {
                            prioritize(thread->waiting_mutex);
                            thread->waiting_mutex = nullptr;
                        }
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
            if (current_thread->state == thread::state_t::running || current_thread->state == thread::state_t::ready)
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
            return reinterpret_cast<tcb_t*>(threads[0]);
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

                if (threads[ix]->state == thread::state_t::ready)
                {
                    reinterpret_cast<tcb_t*>(threads[ix])->age();
                }
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

        if constexpr (scheduler_min_cycles_remaining != 0)
        {
            if (artos::port::get_cycles_remaining() < scheduler_min_cycles_remaining)
            {
                // The current thread may already be blocked. Remove it from the
                // ready queue before idling until the next tick.
                if (current_thread->state != thread::state_t::running && current_thread->state != thread::state_t::ready
                    && current_thread->ready.queue != nullptr)
                {
                    auto* queue = current_thread->ready.queue;
                    queue->remove(current_thread->ready);
                    if (queue->empty())
                    {
                        clear_priority_ready(current_thread->priority);
                    }
                }

                next_thread = threads[0];
                if (next_thread != current_thread)
                {
                    artos::port::pendsvset();
                }
                else
                {
                    reinterpret_cast<tcb_t*>(current_thread)->keep_running();
                }

                return; // don't remove
            }
        }

        check_deadlines();

        auto* selected = select_next(scheduler);
        // Refresh even if a previously requested PendSV is already pending
        // and a new tick now selects the current thread again.
        next_thread = selected;

        age();

        reinterpret_cast<tcb_t*>(next_thread)->reset_priority();

        if (selected != current_thread)
        {
            artos::port::pendsvset();
        }
        else
        {
            reinterpret_cast<tcb_t*>(current_thread)->keep_running();
        }
    }
}
