#pragma once

#include <cstdint>

namespace artos {
    enum class wake_reason_t : std::uint8_t
    {
        none,
        signaled,
        timeout,
        cancelled
    };
}