#pragma once

#include <artos.h>

namespace artos::kernel::internal {
    constexpr bool deadline_reached(tick_t now, tick_t deadline) noexcept
    {
        return tick_t(now - deadline) < (tick_t { 1 } << 31);
    }

}
