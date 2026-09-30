#pragma once

#include <cstdint>

namespace board {
    void init() noexcept;

    namespace stopwatch {
        void                        init() noexcept;
        [[nodiscard]] std::uint32_t count() noexcept;
        [[nodiscard]] std::uint32_t clock_hz() noexcept;
    }
}
