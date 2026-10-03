#include <cassert>
#include <optional>
#include <utility>

#include <artos.h>
#include <defs.h>
#include <mutex.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>

namespace artos::kernel {
    using namespace internal;

    namespace locked {
        void block_current(wait_queue_t& queue, std::optional<tick_time_t> deadline) noexcept
        {
            auto* thread = current_thread;

            assert(thread);
            assert(thread->state == thread::state_t::running);
            assert(thread->waiter.thread == thread);
            assert(thread->waiter.queue == nullptr);

            thread->waiter.wake = wake_reason_t::none;
            thread->deadline = deadline.value_or(tick_time_t {});
            queue.push_back(thread->waiter);

            if (thread->waiting_mutex)
            {
                kernel::internal::prioritize(thread->waiting_mutex);
            }

            if (deadline)
            {
                thread->state = thread::state_t::blocked;
                block_sleep_thread(thread);
            }
            else
            {
                thread->state = thread::state_t::indefinite;
            }

            sched();
        }

        [[nodiscard]] wake_reason_t block_current_until(wait_queue_t&              queue,
                                                        std::optional<tick_time_t> deadline,
                                                        critical_section_t         crit_section) noexcept
        {
            {
                auto cs = std::move(crit_section);
                block_current(queue, deadline);
            }

            return current_thread->waiter.wake;
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

            wait_queue_node_t* selected = &*it;

            while (it != queue.end())
            {
                if (selected->thread->priority > it->thread->priority)
                {
                    selected = &*it;
                }

                ++it;
            }

            auto* thread = selected->thread;

            assert(thread->state == thread::state_t::blocked || thread->state == thread::state_t::indefinite);
            assert(selected->wake == wake_reason_t::none);

            queue.remove(*selected);
            selected->wake = wake_reason_t::signaled;
            thread->waiting_mutex = nullptr;

            thread::internal::make_thread_ready(thread);

            if (thread->block_sleep.queue)
            {
                thread->block_sleep.queue->remove(thread->block_sleep);
            }

            return thread->priority < current_thread->priority;
        }

        bool wake_all(wait_queue_t& queue, wake_reason_t reason) noexcept
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

                assert(thread->state == thread::state_t::blocked || thread->state == thread::state_t::indefinite);
                assert(it->wake == wake_reason_t::none);

                it->wake = reason;
                thread->waiting_mutex = nullptr;

                thread::internal::make_thread_ready(thread);

                if (thread->block_sleep.queue)
                {
                    thread->block_sleep.queue->remove(thread->block_sleep);
                }

                higher_priority_thread_woken |= thread->priority < current_thread->priority;

                ++it;
            }

            queue.clear();

            return higher_priority_thread_woken;
        }
    }

}
