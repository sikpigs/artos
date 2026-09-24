#include "internal/scheduler.h"
#include "internal/time.h"

#include <cstring>

#include <artos.h>
#include <queue.h>
#include <wait_queue.h>

namespace artos::internal {
    bool update_deadline(tick_time_t& deadline) noexcept
    {
        const auto now = kernel::locked::tick_time();
        if (kernel::internal::deadline_reached(now, deadline))
        {
            return false;
        }

        const auto remaining = deadline - now;
        deadline = now + remaining;
        return true;
    }

    [[nodiscard]] bool queue_core_t::push_front(const void* element, tick_t ticks_to_wait) noexcept
    {
        tick_time_t deadline = kernel::tick_time() + ticks_to_wait;

        while (true)
        {
            critical_section_t cs {};
            if (is_full())
            {
                if (!update_deadline(deadline))
                {
                    return false;
                }

                if (kernel::locked::block_current(writer_queue, deadline, std::move(cs)) != wake_reason_t::signaled)
                {
                    return false;
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

                return true;
            }
        }
    }

    [[nodiscard]] bool queue_core_t::push_back(const void* element, tick_t ticks_to_wait) noexcept
    {
        tick_time_t deadline = kernel::tick_time() + ticks_to_wait;

        while (true)
        {
            critical_section_t cs {};
            if (is_full())
            {
                if (!update_deadline(deadline))
                {
                    return false;
                }

                if (kernel::locked::block_current(writer_queue, deadline, std::move(cs)) != wake_reason_t::signaled)
                {
                    return false;
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

                return true;
            }
        }
    }

    [[nodiscard]] bool queue_core_t::pop_front(void* element, tick_t ticks_to_wait) noexcept
    {
        tick_time_t deadline = kernel::tick_time() + ticks_to_wait;

        while (true)
        {
            critical_section_t cs {};
            if (is_empty())
            {
                if (!update_deadline(deadline))
                {
                    return false;
                }

                if (kernel::locked::block_current(reader_queue, deadline, std::move(cs)) != wake_reason_t::signaled)
                {
                    return false;
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

                return true;
            }
        }
    }

    [[nodiscard]] bool queue_core_t::pop_back(void* element, tick_t ticks_to_wait) noexcept
    {
        tick_time_t deadline = kernel::tick_time() + ticks_to_wait;

        while (true)
        {
            critical_section_t cs {};
            if (is_empty())
            {
                if (!update_deadline(deadline))
                {
                    return false;
                }

                if (kernel::locked::block_current(reader_queue, deadline, std::move(cs)) != wake_reason_t::signaled)
                {
                    return false;
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

                return true;
            }
        }
    }
}
