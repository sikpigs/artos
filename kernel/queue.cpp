#include <cassert>
#include <cstring>
#include <optional>
#include <utility>

#include <artos.h>
#include <defs.h>
#include <queue.h>
#include <wait_queue.h>

#include <internal/scheduler.h>
#include <internal/time.h>

namespace artos::internal {
    [[nodiscard]] queue_result_t queue_core_t::push_front(const void*           element,
                                                          std::optional<tick_t> ticks_to_wait) noexcept
    {
        const operation_guard_t operation { *this };
        if (!operation.entered())
        {
            return queue_result_t::closed;
        }

        tick_time_t deadline = kernel::tick_time() + ticks_to_wait.value_or(0);

        while (true)
        {
            critical_section_t cs {};
            if (state == state_t::closed)
            {
                return queue_result_t::closed;
            }

            if (is_full())
            {
                if (ticks_to_wait && !kernel::internal::update_deadline(deadline))
                {
                    return queue_result_t::timeout;
                }

                wake_reason_t reason;
                if (ticks_to_wait)
                {
                    reason = kernel::locked::block_current(writer_queue, deadline, std::move(cs));
                }
                else
                {
                    reason = kernel::locked::block_current(writer_queue, wait_forever, std::move(cs));
                }

                if (reason != wake_reason_t::signaled)
                {
                    return reason == wake_reason_t::cancelled ? queue_result_t::closed : queue_result_t::timeout;
                }
            }
            else
            {
                head = (head - 1 + capacity) % capacity;

                auto destination = buffer + (head * elem_size);
                std::memcpy(destination, element, elem_size);
                ++size;

                if (artos::kernel::locked::wake_one(reader_queue))
                {
                    kernel::internal::sched();
                }

                return queue_result_t::success;
            }
        }
    }

    [[nodiscard]] queue_result_t queue_core_t::push_back(const void*           element,
                                                         std::optional<tick_t> ticks_to_wait) noexcept
    {
        const operation_guard_t operation { *this };
        if (!operation.entered())
        {
            return queue_result_t::closed;
        }

        tick_time_t deadline = kernel::tick_time() + ticks_to_wait.value_or(0);

        while (true)
        {
            critical_section_t cs {};
            if (state == state_t::closed)
            {
                return queue_result_t::closed;
            }

            if (is_full())
            {
                if (ticks_to_wait && !kernel::internal::update_deadline(deadline))
                {
                    return queue_result_t::timeout;
                }

                wake_reason_t reason;
                if (ticks_to_wait)
                {
                    reason = kernel::locked::block_current(writer_queue, deadline, std::move(cs));
                }
                else
                {
                    reason = kernel::locked::block_current(writer_queue, wait_forever, std::move(cs));
                }

                if (reason != wake_reason_t::signaled)
                {
                    return reason == wake_reason_t::cancelled ? queue_result_t::closed : queue_result_t::timeout;
                }
            }
            else
            {
                auto* destination = buffer + (tail * elem_size);
                std::memcpy(destination, element, elem_size);
                tail = (tail + 1) % capacity;
                ++size;

                if (kernel::locked::wake_one(reader_queue))
                {
                    kernel::internal::sched();
                }

                return queue_result_t::success;
            }
        }
    }

    [[nodiscard]] queue_result_t queue_core_t::pop_front(void* element, std::optional<tick_t> ticks_to_wait) noexcept
    {
        const operation_guard_t operation { *this };
        if (!operation.entered())
        {
            return queue_result_t::closed;
        }

        tick_time_t deadline = kernel::tick_time() + ticks_to_wait.value_or(0);

        while (true)
        {
            critical_section_t cs {};
            if (state == state_t::closed)
            {
                return queue_result_t::closed;
            }

            if (is_empty())
            {
                if (ticks_to_wait && !kernel::internal::update_deadline(deadline))
                {
                    return queue_result_t::timeout;
                }

                wake_reason_t reason;
                if (ticks_to_wait)
                {
                    reason = kernel::locked::block_current(reader_queue, deadline, std::move(cs));
                }
                else
                {
                    reason = kernel::locked::block_current(reader_queue, wait_forever, std::move(cs));
                }

                if (reason != wake_reason_t::signaled)
                {
                    return reason == wake_reason_t::cancelled ? queue_result_t::closed : queue_result_t::timeout;
                }
            }
            else
            {
                const auto* source = buffer + (head * elem_size);
                std::memcpy(element, source, elem_size);
                head = (head + 1) % capacity;
                --size;

                if (kernel::locked::wake_one(writer_queue))
                {
                    kernel::internal::sched();
                }

                return queue_result_t::success;
            }
        }
    }

    [[nodiscard]] queue_result_t queue_core_t::pop_back(void* element, std::optional<tick_t> ticks_to_wait) noexcept
    {
        const operation_guard_t operation { *this };
        if (!operation.entered())
        {
            return queue_result_t::closed;
        }

        tick_time_t deadline = kernel::tick_time() + ticks_to_wait.value_or(0);

        while (true)
        {
            critical_section_t cs {};
            if (state == state_t::closed)
            {
                return queue_result_t::closed;
            }

            if (is_empty())
            {
                if (ticks_to_wait && !kernel::internal::update_deadline(deadline))
                {
                    return queue_result_t::timeout;
                }

                wake_reason_t reason;
                if (ticks_to_wait)
                {
                    reason = kernel::locked::block_current(reader_queue, deadline, std::move(cs));
                }
                else
                {
                    reason = kernel::locked::block_current(reader_queue, wait_forever, std::move(cs));
                }

                if (reason != wake_reason_t::signaled)
                {
                    return reason == wake_reason_t::cancelled ? queue_result_t::closed : queue_result_t::timeout;
                }
            }
            else
            {
                tail = (tail - 1 + capacity) % capacity;
                const auto* source = buffer + (tail * elem_size);
                std::memcpy(element, source, elem_size);
                --size;

                if (kernel::locked::wake_one(writer_queue))
                {
                    kernel::internal::sched();
                }

                return queue_result_t::success;
            }
        }
    }

    [[nodiscard]] queue_result_t queue_core_t::try_push_front(const void* element,
                                                              bool*       higher_priority_thread_woken) noexcept
    {
        const critical_section_t cs {};

        if (state == state_t::closed)
        {
            return queue_result_t::closed;
        }

        if (is_full())
        {
            return queue_result_t::full;
        }

        head = (head - 1 + capacity) % capacity;
        auto destination = buffer + (head * elem_size);
        std::memcpy(destination, element, elem_size);
        ++size;

        const auto woken = artos::kernel::locked::wake_one(reader_queue);
        if (higher_priority_thread_woken && woken)
        {
            *higher_priority_thread_woken = woken;
        }

        return queue_result_t::success;
    }

    [[nodiscard]] queue_result_t queue_core_t::try_push_back(const void* element,
                                                             bool*       higher_priority_thread_woken) noexcept
    {
        const critical_section_t cs {};

        if (state == state_t::closed)
        {
            return queue_result_t::closed;
        }

        if (is_full())
        {
            return queue_result_t::full;
        }

        auto* destination = buffer + (tail * elem_size);
        std::memcpy(destination, element, elem_size);
        tail = (tail + 1) % capacity;
        ++size;

        const auto woken = artos::kernel::locked::wake_one(reader_queue);
        if (higher_priority_thread_woken && woken)
        {
            *higher_priority_thread_woken = woken;
        }

        return queue_result_t::success;
    }

    [[nodiscard]] queue_result_t queue_core_t::try_pop_front(void* element, bool* higher_priority_thread_woken) noexcept
    {
        const critical_section_t cs {};

        if (state == state_t::closed)
        {
            return queue_result_t::closed;
        }

        if (is_empty())
        {
            return queue_result_t::empty;
        }

        const auto* source = buffer + (head * elem_size);
        std::memcpy(element, source, elem_size);
        head = (head + 1) % capacity;
        --size;

        const auto woken = artos::kernel::locked::wake_one(writer_queue);
        if (higher_priority_thread_woken && woken)
        {
            *higher_priority_thread_woken = woken;
        }

        return queue_result_t::success;
    }

    [[nodiscard]] queue_result_t queue_core_t::try_pop_back(void* element, bool* higher_priority_thread_woken) noexcept
    {
        const critical_section_t cs {};

        if (state == state_t::closed)
        {
            return queue_result_t::closed;
        }

        if (is_empty())
        {
            return queue_result_t::empty;
        }

        tail = (tail - 1 + capacity) % capacity;
        const auto* source = buffer + (tail * elem_size);
        std::memcpy(element, source, elem_size);
        --size;

        const auto woken = artos::kernel::locked::wake_one(writer_queue);
        if (higher_priority_thread_woken && woken)
        {
            *higher_priority_thread_woken = woken;
        }

        return queue_result_t::success;
    }

    queue_core_t::operation_guard_t::operation_guard_t(queue_core_t& owner) noexcept
        : queue { owner }
    {
        const critical_section_t cs {};
        if (owner.state == state_t::open)
        {
            ++owner.active_operations;
            active = true;
        }
    }

    queue_core_t::operation_guard_t::~operation_guard_t() noexcept
    {
        if (active)
        {
            queue.finish_operation();
        }
    }

    void queue_core_t::finish_operation() noexcept
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

    void queue_core_t::close() noexcept
    {
        const critical_section_t cs {};
        if (state == state_t::closed)
        {
            return;
        }

        state = state_t::closed;
        const bool wake_writer = kernel::locked::wake_all(writer_queue, wake_reason_t::cancelled);
        const bool wake_reader = kernel::locked::wake_all(reader_queue, wake_reason_t::cancelled);
        if (wake_writer || wake_reader)
        {
            kernel::internal::sched();
        }
    }

    void queue_core_t::close_and_wait() noexcept
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

    void queue_core_t::close_and_delete() noexcept
    {
        const critical_section_t cs {};
        delete_on_close = true;
        if (state != state_t::closed)
        {
            state = state_t::closed;

            const bool wake_writer = kernel::locked::wake_all(writer_queue, wake_reason_t::cancelled);
            const bool wake_reader = kernel::locked::wake_all(reader_queue, wake_reason_t::cancelled);
            if (wake_writer || wake_reader)
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
