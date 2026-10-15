#include <atomic>
#include <cassert>
#include <cstdint>
#include <optional>

#include <port/cpu_port.h>

#include <artos.h>
#include <mutex.h>

#include <internal/container_of.h>
#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/time.h>

namespace artos {
    using namespace kernel::internal;
    using namespace thread::internal;

    namespace {
        tcb_base_t* get_owner(thread::id_t owner) noexcept
        {
            auto ix = tcb_base_t::id_layout::extract_id(owner);
            assert(ix < threads.size());
            auto* thread = threads[ix];
            assert(thread == nullptr || thread->id == owner);
            return thread;
        }
    }

    mutex_result_t mutex_t::try_lock() noexcept
    {
        if (cpu::port::is_in_irq())
        {
            return mutex_result_t::unsupported;
        }

        const critical_section_t cs {};

        std::uint_fast32_t expected = 0;

        const bool acquired = state.compare_exchange_strong(expected, 1);

        if (!acquired)
        {
            return mutex_result_t::timeout;
        }

        owner = thread::get_current_thread();
        auto* thread = get_owner(owner);
        thread->mutexes.push_back(this);
        kernel::internal::recalculate_priority(thread->id);

        return mutex_result_t::success;
    }

    mutex_result_t mutex_t::release() noexcept
    {
        if (cpu::port::is_in_irq())
        {
            return mutex_result_t::unsupported;
        }

        const critical_section_t cs {};

        if (owner == thread::invalid_id)
        {
            return mutex_result_t::not_locked;
        }

        if (owner != thread::get_current_thread())
        {
            return mutex_result_t::no_permission;
        }

        std::uint_fast32_t expected = 1;

        const bool was_held = state.compare_exchange_strong(expected, 0);

        if (!was_held)
        {
            [[maybe_unused]] auto* thread = get_owner(owner);
            assert(thread == nullptr || thread->mutexes.empty());
            owner = thread::invalid_id;
            return mutex_result_t::not_locked;
        }

        unhook();
        prioritize();

        owner = thread::invalid_id;

        if (kernel::locked::wake_one(waiters))
        {
            kernel::internal::sched();
        }

        return mutex_result_t::success;
    }

    mutex_result_t mutex_t::lock(std::optional<tick_t> ticks_to_wait) noexcept
    {
        if (cpu::port::is_in_irq())
        {
            return mutex_result_t::unsupported;
        }

        tick_time_t deadline = kernel::tick_time() + ticks_to_wait.value_or(0);

        while (true)
        {
            critical_section_t cs {};

            if (owner != thread::invalid_id)
            {
                if (ticks_to_wait && !kernel::internal::update_deadline(deadline))
                {
                    return mutex_result_t::timeout;
                }

                auto* owning_thread = get_owner(owner);
                while (owning_thread != current_thread)
                {
                    if (owning_thread->waiting_mutex == nullptr)
                    {
                        break;
                    }

                    owning_thread = get_owner(owning_thread->waiting_mutex->owner);
                }

                if (owning_thread == current_thread)
                {
                    return mutex_result_t::deadlock_detected;
                }

                current_thread->waiting_mutex = this;

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
                    return mutex_result_t::timeout;
                }
            }
            else
            {
                if (const auto result = try_lock(); result == mutex_result_t::success)
                {
                    return result;
                }
            }
        }
    }

    mutex_result_t mutex_t::lock(tick_t ticks_to_wait) noexcept
    {
        return lock(std::make_optional(ticks_to_wait));
    }

    mutex_result_t mutex_t::lock(wait_forever_t) noexcept
    {
        return lock(std::nullopt);
    }

    void mutex_t::unhook() noexcept
    {
        auto* thread = get_owner(owner);
        assert(thread);
        thread->mutexes.erase(this);
    }

    void mutex_t::prioritize() noexcept
    {
        auto* owning_thread = get_owner(owner);
        assert(owning_thread);

        while (owning_thread != nullptr)
        {
            kernel::internal::recalculate_priority(owning_thread->id);
            owning_thread = owning_thread->waiting_mutex ? get_owner(owning_thread->waiting_mutex->owner) : nullptr;
        }
    }
}
