#pragma once

#include <cstddef>
#include <cstdint>

namespace build_config::artos {
    inline constexpr std::uint32_t tick_hz = 1000;
    inline constexpr std::uint32_t priority_levels = 16;
    inline constexpr std::size_t   max_threads = 8;
}
