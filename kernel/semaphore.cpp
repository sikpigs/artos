#include <cassert>
#include <cpu_port.h>
#include <optional>

#include <artos.h>
#include <defs.h>
#include <semaphore.h>
#include <wait_queue.h>

#include <internal/scheduler.h>
#include <internal/time.h>

namespace artos {
    semaphore_t::operation_guard_t::operation_guard_t(semaphore_t& owner) noexcept
        : sem { owner }
    {
        const critical_section_t cs {};
        if (owner.state == state_t::open)
        {
            ++owner.active_operations;
            active = true;
        }
    }

    semaphore_t::operation_guard_t::~operation_guard_t() noexcept
    {
        if (active)
        {
            sem.finish_operation();
        }
    }

    semaphore_result_t semaphore_t::try_acquire() noexcept
    {
        const critical_section_t cs {};

        if (state == state_t::closed)
        {
            return semaphore_result_t::closed;
        }

        if (count == maximum)
        {
            return semaphore_result_t::full;
        }

        ++count;

        return semaphore_result_t::success;
    }

    semaphore_result_t semaphore_t::acquire(tick_t ticks_to_wait) noexcept
    {
        return acquire(std::make_optional(ticks_to_wait));
    }

    semaphore_result_t semaphore_t::acquire(wait_forever_t) noexcept
    {
        return acquire(std::nullopt);
    }

    semaphore_result_t semaphore_t::acquire(std::optional<tick_t> ticks_to_wait) noexcept
    {
        if (cpu::port::is_in_irq())
        {
            return semaphore_result_t::unsupported;
        }

        const operation_guard_t operation { *this };
        if (!operation.entered())
        {
            return semaphore_result_t::closed;
        }

        tick_time_t deadline = kernel::tick_time() + ticks_to_wait.value_or(0);

        while (true)
        {
            critical_section_t cs {};

            if (state == state_t::closed)
            {
                return semaphore_result_t::closed;
            }

            if (count == maximum)
            {
                if (ticks_to_wait && !kernel::internal::update_deadline(deadline))
                {
                    return semaphore_result_t::timeout;
                }

                wake_reason_t reason;
                if (ticks_to_wait)
                {
                    reason = kernel::locked::block_current(waiters, deadline, std::move(cs));
                }
                else
                {
                    reason = kernel::locked::block_current(waiters, wait_forever, std::move(cs));
                }

                if (reason != wake_reason_t::signaled)
                {
                    return reason == wake_reason_t::cancelled ? semaphore_result_t::closed
                                                              : semaphore_result_t::timeout;
                }
            }
            else
            {
                ++count;
                break;
            }
        }

        return semaphore_result_t::success;
    }

    semaphore_result_t semaphore_t::release(const operation_guard_t& operation) noexcept
    {
        if (!operation.entered())
        {
            return semaphore_result_t::closed;
        }

        const critical_section_t cs {};
        if (state == state_t::closed)
        {
            return semaphore_result_t::closed;
        }

        if (count == 0)
        {
            return semaphore_result_t::success;
        }

        --count;

        return semaphore_result_t::success;
    }

    semaphore_result_t semaphore_t::release() noexcept
    {
        const operation_guard_t  operation { *this };
        const critical_section_t cs {};

        auto result = release(operation);

        if (result == semaphore_result_t::success)
        {
            if (kernel::locked::wake_one(waiters))
            {
                kernel::internal::sched();
            }
        }

        return result;
    }

    semaphore_result_t semaphore_t::release_from_isr(bool* higher_priority_thread_woken) noexcept
    {
        const operation_guard_t  operation { *this };
        const critical_section_t cs {};

        auto result = release(operation);

        if (result == semaphore_result_t::success)
        {
            const auto woken = kernel::locked::wake_one(waiters);
            if (higher_priority_thread_woken && woken)
            {
                *higher_priority_thread_woken = woken;
            }
        }

        return result;
    }

    void semaphore_t::finish_operation() noexcept
    {
        const critical_section_t cs {};
        assert(active_operations > 0);
        --active_operations;
        const auto active_operations_complete = active_operations == 0;
        if (state == state_t::closed && active_operations_complete && kernel::locked::wake_all(close_queue))
        {
            kernel::internal::sched();
        }

        if (active_operations_complete && delete_on_close)
        {
            delete this;
        }
    }

    void semaphore_t::close() noexcept
    {
        const critical_section_t cs {};
        if (state == state_t::closed)
        {
            return;
        }

        state = state_t::closed;
        const bool woken = kernel::locked::wake_all(waiters, wake_reason_t::cancelled);
        if (woken)
        {
            kernel::internal::sched();
        }
    }

    void semaphore_t::close_and_wait() noexcept
    {
        close();
        while (true)
        {
            critical_section_t cs {};
            if (active_operations == 0)
            {
                return;
            }

            [[maybe_unused]] const auto reason
                = kernel::locked::block_current(close_queue, wait_forever, std::move(cs));
            assert(reason == wake_reason_t::signaled);
        }
    }

    void semaphore_t::close_and_delete() noexcept
    {
        const critical_section_t cs {};
        delete_on_close = true;
        if (state != state_t::closed)
        {
            state = state_t::closed;

            const bool woken = kernel::locked::wake_all(waiters, wake_reason_t::cancelled);
            if (woken)
            {
                kernel::internal::sched();
            }
        }

        if (active_operations == 0)
        {
            delete this;
        }
    }
}