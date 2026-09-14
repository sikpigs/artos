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
            auto* node = queue.front();
            if (!node)
            {
                return false;
            }

            auto* thread = node->thread;

            assert(thread->state == thread::internal::thread_state_t::blocked);
            assert(node->wake == wake_reason_t::none);

            queue.remove(*node);
            node->wake = wake_reason_t::signaled;
            thread->state = thread::internal::thread_state_t::ready;

            // todo reconsider scheduling
            return true;
        }
    }

}
