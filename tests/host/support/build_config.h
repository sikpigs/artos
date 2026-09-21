#pragma once

#include <cstddef>
#include <cstdint>

namespace build_config::artos {
    struct fixed_priority_round_robin_config
    {
    };
    struct strict_fixed_priority_config
    {
    };
    struct priority_aging_config
    {
        std::uint32_t ticks;
    };

    inline constexpr std::uint32_t tick_hz = 1000;
    inline constexpr std::uint32_t priority_levels = 16;
    inline constexpr std::size_t   max_threads = 8;
    inline constexpr std::size_t   max_active_guards = 2;
    inline constexpr auto          scheduler = fixed_priority_round_robin_config {};
}
