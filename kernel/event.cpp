#include <cassert>
#include <expected>
#include <internal/operation.h>
#include <optional>
#include <variant>

#include <artos.h>
#include <event.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/time.h>

namespace artos {
    using namespace kernel::internal;
    using namespace kernel::locked;

    bool event_t::satisfied(any_or_all_t bits) noexcept
    {
        if (std::holds_alternative<any_t>(bits))
        {
            if (bitset & std::get<any_t>(bits).bits)
            {
                return true;
            }
        }
        else
        {
            if ((bitset & std::get<all_t>(bits).bits) == std::get<all_t>(bits).bits)
            {
                return true;
            }
        }

        return false;
    }

    void event_t::clear(any_or_all_t bits) noexcept
    {
        if (std::holds_alternative<any_t>(bits))
        {
            if (bitset & std::get<any_t>(bits).bits)
            {
                bitset &= ~std::get<any_t>(bits).bits;
            }
        }
        else
        {
            if ((bitset & std::get<all_t>(bits).bits) == std::get<all_t>(bits).bits)
            {
                bitset &= ~std::get<all_t>(bits).bits;
            }
        }
    }

    bool event_t::notify_waiters() noexcept
    {
        // All recipients must be selected against the same state. Preserve
        // their results before making them ready, and clear only after the
        // wake helper has checked every predicate against that state.
        bits_t reset_mask = 0;
        for (auto& waiter : waiters)
        {
            auto* thread = waiter.thread;
            if (satisfied(thread->waiting_for))
            {
                thread->event_snapshot = bitset;
                if (thread->event_reset)
                {
                    reset_mask |= std::visit([](auto requested) { return requested.bits; }, thread->waiting_for);
                }
            }
        }
        const bool woken = wake_all(waiters);
        bitset &= ~reset_mask;
        return woken;
    }

    event_result_t event_t::post(bits_t bits) noexcept
    {
        if (cpu::port::is_in_irq())
        {
            return artos::unexpected(event_error_t::unsupported);
        }

        const critical_section_t cs {};

        if (state == state_t::closed)
        {
            return artos::unexpected(event_error_t::closed);
        }

        auto current = bitset;

        bitset |= bits;

        if (notify_waiters())
        {
            sched();
        }

        return current;
    }

    event_result_t event_t::set(bits_t bits) noexcept
    {
        if (cpu::port::is_in_irq())
        {
            return artos::unexpected(event_error_t::unsupported);
        }

        const critical_section_t cs {};

        if (state == state_t::closed)
        {
            return artos::unexpected(event_error_t::closed);
        }

        auto current = bitset;

        bitset = bits;

        if (notify_waiters())
        {
            sched();
        }

        return current;
    }

    event_result_t event_t::post_from_isr(bits_t bits, bool* higher_priority_thread_woken) noexcept
    {
        const critical_section_t cs {};
        if (state == state_t::closed)
        {
            return artos::unexpected(event_error_t::closed);
        }

        auto current = bitset;

        bitset |= bits;

        const bool woken = notify_waiters();
        if (woken && higher_priority_thread_woken)
        {
            *higher_priority_thread_woken = true;
        }

        return current;
    }

    event_result_t event_t::set_from_isr(bits_t bits, bool* higher_priority_thread_woken) noexcept
    {
        const critical_section_t cs {};
        if (state == state_t::closed)
        {
            return artos::unexpected(event_error_t::closed);
        }

        auto current = bitset;

        bitset = bits;

        const bool woken = notify_waiters();
        if (woken && higher_priority_thread_woken)
        {
            *higher_priority_thread_woken = true;
        }

        return current;
    }

    event_result_t event_t::wait(any_or_all_t bits, bool reset, std::optional<tick_t> ticks_to_wait) noexcept
    {
        if (cpu::port::is_in_irq())
        {
            return artos::unexpected(event_error_t::unsupported);
        }

        const operation_guard_t operation { *this };
        if (!operation.entered())
        {
            return artos::unexpected(event_error_t::closed);
        }

        tick_time_t        deadline = kernel::tick_time() + ticks_to_wait.value_or(0);
        critical_section_t cs {};
        if (state == state_t::closed)
        {
            return artos::unexpected(event_error_t::closed);
        }
        if (satisfied(bits))
        {
            const auto result = bitset;
            if (reset)
            {
                clear(bits);
            }
            return result;
        }
        if (ticks_to_wait && !kernel::internal::update_deadline(deadline))
        {
            return artos::unexpected(event_error_t::timeout);
        }

        auto* waiter = current_thread;
        assert(waiter);
        waiter->waiting_event = this;
        waiter->waiting_for = bits;
        waiter->event_reset = reset;

        const auto reason = ticks_to_wait ? kernel::locked::block_current(waiters, deadline, std::move(cs))
                                          : kernel::locked::block_current(waiters, wait_forever, std::move(cs));
        if (reason != wake_reason_t::signaled)
        {
            return artos::unexpected(reason == wake_reason_t::cancelled ? event_error_t::closed
                                                                        : event_error_t::timeout);
        }

        // Delivery has already captured the result and applied any reset.
        // Rechecking shared state here could lose it or consume a newer post.
        return waiter->event_snapshot;
    }

    event_result_t event_t::wait_for(any_t bits, bool reset, tick_t ticks_to_wait) noexcept
    {
        return wait(any_or_all_t(bits), reset, std::make_optional(ticks_to_wait));
    }

    event_result_t event_t::wait_for(any_t bits, bool reset, wait_forever_t) noexcept
    {
        return wait(any_or_all_t(bits), reset, std::nullopt);
    }

    event_result_t event_t::wait_for(all_t bits, bool reset, tick_t ticks_to_wait) noexcept
    {
        return wait(any_or_all_t(bits), reset, std::make_optional(ticks_to_wait));
    }

    event_result_t event_t::wait_for(all_t bits, bool reset, wait_forever_t) noexcept
    {
        return wait(any_or_all_t(bits), reset, std::nullopt);
    }

    event_t::operation_guard_t::operation_guard_t(event_t& owner) noexcept
    {
        const critical_section_t cs {};
        if (owner.state == state_t::open)
        {
            ++owner.active_operations;
            active = true;
            thread::internal::register_operation(
                cleanup, &owner, [](void* context) noexcept { static_cast<event_t*>(context)->finish_operation(); });
        }
    }

    event_t::operation_guard_t::~operation_guard_t() noexcept
    {
        if (active)
        {
            thread::internal::complete_operation(cleanup);
        }
    }

    void event_t::finish_operation() noexcept
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

    void event_t::close() noexcept
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

    void event_t::close_and_wait() noexcept
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

    void event_t::close_and_delete() noexcept
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