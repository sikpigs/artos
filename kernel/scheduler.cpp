#include "internal/scheduler.h"
#include "internal/thread.h"
#include "internal/time.h"

#include <artos_port.h>
#include <wait.h>

namespace artos::thread::internal {
    std::array<ready_queue_t, build_config::artos::priority_levels> ready_queue;
    std::array<tcb_t*, build_config::artos::max_threads>            threads;
    std::size_t                                                     thread_count = 0;
    tcb_t*                                                          current_thread = nullptr;
    tcb_t*                                                          next_thread = nullptr;

    extern "C" bool artos_context_started() noexcept
    {
        return current_thread != nullptr;
    }

    extern "C" stack_word_t* artos_context_start() noexcept
    {
        current_thread = next_thread;
        next_thread = nullptr;
        current_thread->state = thread_state_t::running;
        return current_thread->sp;
    }

    extern "C" stack_word_t* artos_context_switch(stack_word_t* outgoing_sp) noexcept
    {
        if (current_thread != nullptr)
        {
            current_thread->sp = outgoing_sp;
            if (current_thread->state == thread_state_t::running)
            {
                current_thread->state = thread_state_t::ready;
            }
        }

        current_thread = next_thread;
        current_thread->state = thread_state_t::running;
        next_thread = nullptr;

        return current_thread->sp;
    }
}

namespace artos::kernel::internal {
    thread::internal::tcb_t* select_next() noexcept
    {
        using namespace thread::internal;
        // Scan after current for round-robin ties. Slot zero is only a
        // fallback, including when an application has the lowest priority.
        // This also runs on a caller's task stack from sleep_until(). Keep
        // the scan shallow even when Debug disables library inlining.
        std::size_t origin = 0;
        for (std::size_t slot = 1; slot < threads.size(); ++slot)
        {
            if (current_thread != nullptr && threads[slot] == current_thread)
            {
                origin = slot;
                break;
            }
        }

        tcb_t*     selected = nullptr;
        const auto now = kernel::tick_time();

        for (std::size_t offset = 1; offset <= threads.size(); ++offset)
        {
            const auto slot = (origin + offset) % threads.size();
            auto*      candidate = threads[slot];

            if (slot == 0 || candidate == nullptr)
            {
                continue;
            }

            if ((candidate->state == thread_state_t::sleeping || candidate->state == thread_state_t::blocked)
                && deadline_reached(now, candidate->deadline))
            {
                if (candidate->state == thread_state_t::blocked)
                {
                    candidate->waiter.queue->remove(candidate->waiter);
                    candidate->waiter.wake = wake_reason_t::timeout;
                }

                candidate->state = thread_state_t::ready;
            }

            if (candidate->state != thread_state_t::ready && candidate->state != thread_state_t::running)
            {
                continue;
            }

            if (selected == nullptr || candidate->priority < selected->priority)
            {
                selected = candidate;
            }
        }

        return selected != nullptr ? selected : threads[0];
    }

    void sched() noexcept
    {
        // Leave the PendSV startup selection intact until a thread is running.
        if (thread::internal::current_thread == nullptr)
        {
            return;
        }
        auto* selected = select_next();
        // Refresh even if a previously requested PendSV is already pending
        // and a new tick now selects the current thread again.
        thread::internal::next_thread = selected;
        if (selected != thread::internal::current_thread)
        {
            artos::port::pendsvset();
        }
    }

}
