#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace build_config::artos {
    struct fixed_priority_round_robin_config
    {
        std::optional<std::uint32_t> priority_aging_ticks;
    };

    inline constexpr std::uint32_t tick_hz = 1000;
    inline constexpr std::uint32_t scheduler_min_cycles_remaining = 500;
    inline constexpr std::uint32_t priority_levels = 16;
    inline constexpr std::size_t   max_threads = 8;
    inline constexpr std::size_t   max_active_guards = 2;
    inline constexpr auto          scheduler = fixed_priority_round_robin_config {
#if defined(ARTOS_TEST_PRIORITY_AGING)
        .priority_aging_ticks = 2,
#else
        .priority_aging_ticks = std::nullopt,
#endif
    };
}
