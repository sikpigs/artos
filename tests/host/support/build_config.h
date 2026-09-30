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
    inline constexpr bool          thread_stack_hwm = false;
#if defined(ARTOS_TEST_THREAD_STATS)
    inline constexpr bool thread_stats = true;
#else
    inline constexpr bool thread_stats = false;
#endif
    inline constexpr std::uint32_t runtime_history_window_ms = 1000;
    inline constexpr std::size_t   runtime_history_bucket_count = 10;
    static_assert(runtime_history_window_ms % runtime_history_bucket_count == 0);
    inline constexpr std::uint32_t runtime_history_bucket_ms = runtime_history_window_ms / runtime_history_bucket_count;
    inline constexpr std::size_t   max_active_guards = 2;
    inline constexpr auto          scheduler = fixed_priority_round_robin_config {
#if defined(ARTOS_TEST_PRIORITY_AGING)
        .priority_aging_ticks = 2,
#else
        .priority_aging_ticks = std::nullopt,
#endif
    };
}
