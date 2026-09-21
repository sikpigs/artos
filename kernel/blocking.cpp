#include "internal/scheduler.h"
#include "internal/thread.h"

#include <cassert>
#include <optional>

#include <artos.h>
#include <utility>
#include <wait.h>

namespace artos::kernel {
    namespace locked {
        void block_current(wait_queue_t& queue, std::optional<tick_time_t> deadline) noexcept
        {
            auto* thread = thread::internal::current_thread;

            assert(thread);
            assert(thread->state == thread::internal::thread_state_t::running);
            assert(thread->waiter.thread == thread);
            assert(thread->waiter.queue == nullptr);

            thread->waiter.wake = wake_reason_t::none;
            thread->deadline = deadline.value_or(tick_time_t {});
            queue.push_back(thread->waiter);
            if (deadline)
            {
                thread->state = thread::internal::thread_state_t::blocked;
            }
            else
            {
                thread->state = thread::internal::thread_state_t::blocked_forever;
            }

            if (thread->ready.queue)
            {
                auto ready_queue = thread->ready.queue;
                ready_queue->remove(thread->ready);
                if (ready_queue->empty())
                {
                    thread::internal::clear_priority_ready(thread->priority);
                }
            }

            internal::sched();
        }

        [[nodiscard]] wake_reason_t block_current_until(wait_queue_t&              queue,
                                                        std::optional<tick_time_t> deadline,
                                                        critical_section_t         crit_section) noexcept
        {
            {
                auto cs = std::move(crit_section);
                block_current(queue, deadline);
            }

            return thread::internal::current_thread->waiter.wake;
        }

        wake_reason_t block_current(wait_queue_t& queue, const tick_time_t& deadline, critical_section_t cs) noexcept
        {
            if (deadline == immediately)
            {
                return wake_reason_t::timeout;
            }

            return block_current_until(queue, deadline, std::move(cs));
        }

        wake_reason_t block_current(wait_queue_t& queue, wait_forever_t, critical_section_t cs) noexcept
        {
            return block_current_until(queue, std::nullopt, std::move(cs));
        }

        void block_current(wait_queue_t& queue, const tick_time_t& deadline) noexcept
        {
            if (deadline == immediately)
            {
                return;
            }

            return block_current(queue, std::make_optional(deadline));
        }

        void block_current(wait_queue_t& queue, wait_forever_t) noexcept
        {
            return block_current(queue, std::nullopt);
        }

        bool wake_one(wait_queue_t& queue) noexcept
        {
            if (queue.empty())
            {
                return false;
            }

            auto it = queue.begin();

            wait_node_t* selected = &*it;

            while (it != queue.end())
            {
                if (selected->thread->priority > it->thread->priority)
                {
                    selected = &*it;
                }

                ++it;
            }

            auto* thread = selected->thread;

            assert(thread->state == thread::internal::thread_state_t::blocked
                   || thread->state == thread::internal::thread_state_t::blocked_forever);
            assert(selected->wake == wake_reason_t::none);

            queue.remove(*selected);
            selected->wake = wake_reason_t::signaled;
            thread->state = thread::internal::thread_state_t::ready;
            thread::internal::ready_queue[std::to_underlying(thread->priority)].push_back(thread->ready);
            thread::internal::set_priority_ready(thread->priority);

            return thread->priority < thread::internal::current_thread->priority;
        }

        bool wake_all(wait_queue_t& queue) noexcept
        {
            if (queue.empty())
            {
                return false;
            }

            auto it = queue.begin();
            bool higher_priority_thread_woken = false;

            while (it != queue.end())
            {
                auto* thread = it->thread;

                assert(thread->state == thread::internal::thread_state_t::blocked
                       || thread->state == thread::internal::thread_state_t::blocked_forever);
                assert(it->wake == wake_reason_t::none);

                it->wake = wake_reason_t::signaled;
                thread->state = thread::internal::thread_state_t::ready;
                thread::internal::ready_queue[std::to_underlying(thread->priority)].push_back(thread->ready);
                thread::internal::set_priority_ready(thread->priority);

                higher_priority_thread_woken |= thread->priority < thread::internal::current_thread->priority;

                ++it;
            }

            queue.clear();

            return higher_priority_thread_woken;
        }
    }

}
