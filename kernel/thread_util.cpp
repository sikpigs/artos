#include <build_config.h>

#include <internal/thread.h>
#include <internal/thread_state.h>
#include <internal/time.h>

#include <artos.h>

namespace artos::thread::internal {
    using namespace build_config::artos;

    void set_priority_ready(priority_t priority) noexcept
    {
        const auto p = std::to_underlying(priority);
        ready_bitmap[p >> 5U] |= 1U << (p & 31U);
    }

    void clear_priority_ready(priority_t priority) noexcept
    {
        const auto p = std::to_underlying(priority);
        ready_bitmap[p >> 5U] &= ~(1U << (p & 31U));
    }

    std::optional<priority_t> get_highest_set_priority() noexcept
    {
        auto priority = std::to_underlying(priority_t::high);

        for (const auto bitmap : ready_bitmap)
        {
            const auto p = static_cast<std::uint32_t>(std::countr_zero(bitmap));
            priority += static_cast<std::underlying_type_t<priority_t>>(p);
            if (p < std::numeric_limits<std::uint32_t>::digits)
            {
                return static_cast<priority_t>(priority);
            }
        }

        return std::nullopt;
    }

    void make_thread_ready(tcb_base_t* thread) noexcept
    {
        thread->state = thread_state_t::ready;
        ready_queue[std::to_underlying(thread->priority)].push_back(thread->ready);
        set_priority_ready(thread->priority);
    }

    void block_sleep_thread(tcb_base_t* thread) noexcept
    {
        if (thread->ready.queue)
        {
            auto queue = thread->ready.queue;
            queue->remove(thread->ready);
            if (queue->empty())
            {
                clear_priority_ready(thread->priority);
            }
        }

        auto forward_it = block_sleep_queue.begin();

        while (forward_it != block_sleep_queue.end())
        {
            if (kernel::internal::deadline_reached(forward_it->thread->deadline, thread->deadline))
            {
                break;
            }

            ++forward_it;
        }

        if (forward_it == block_sleep_queue.end())
        {
            block_sleep_queue.push_back(thread->block_sleep);
        }
        else
        {
            block_sleep_queue.insert(forward_it, thread->block_sleep);
        }
    }
}
