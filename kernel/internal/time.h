#pragma once

#include <limits>

#include <artos.h>

namespace artos::kernel::internal {
    constexpr bool deadline_reached(const tick_time_t& now, const tick_time_t& deadline) noexcept
    {
        const auto generation_delta = tick_t { now.generation - deadline.generation };
        if (generation_delta != 0)
        {
            // A small positive modular difference means 'now' is in a later
            // generation. A large difference means the deadline is ahead,
            // across generation rollover.
            return generation_delta < (tick_t { 1 } << (std::numeric_limits<tick_t>::digits - 1));
        }

        return now.ticks >= deadline.ticks;
    }

    inline bool update_deadline(tick_time_t& deadline) noexcept
    {
        const auto now = kernel::locked::tick_time();
        if (deadline_reached(now, deadline))
        {
            return false;
        }

        const auto remaining = deadline - now;
        deadline = now + remaining;
        return true;
    }
}
