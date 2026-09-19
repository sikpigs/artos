#include "internal/scheduler.h"

#include <cassert>
#include <wait.h>

namespace artos::kernel {
    namespace locked {
        void prepare_wait(wait_queue_t& queue) noexcept
        {
            auto* thread = thread::internal::current_thread;

            assert(thread);
            assert(thread->state == thread::internal::thread_state_t::running);
            assert(thread->waiter.thread == thread);
            assert(thread->waiter.queue == nullptr);

            thread->waiter.wake = wake_reason_t::none;
            queue.push_back(thread->waiter);
            thread->state = thread::internal::thread_state_t::blocked;
        }

        void block_current(wait_queue_t& queue) noexcept
        {
            prepare_wait(queue);
            internal::sched();
        }

        bool wake_one(wait_queue_t& queue) noexcept
        {
            if (queue.empty())
            {
                return false;
            }

            auto it = queue.begin();

            wait_node_t& selected = *it;

            while (it != queue.end())
            {
                if (selected.thread->priority < it->thread->priority)
                {
                    selected = *it;
                }

                ++it;
            }

            auto* thread = selected.thread;

            assert(thread->state == thread::internal::thread_state_t::blocked);
            assert(selected.wake == wake_reason_t::none);

            queue.remove(selected);
            selected.wake = wake_reason_t::signaled;
            thread->state = thread::internal::thread_state_t::ready;

            return thread->priority > thread::internal::current_thread->priority;
        }
    }

}
