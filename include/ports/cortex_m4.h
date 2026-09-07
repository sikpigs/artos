#pragma once

#include <cstddef>
#include <cstdint>

namespace artos::port {
    using stack_word_t = std::uint32_t;
    inline constexpr std::size_t stack_alignment = 8;
    // Basic exception frame plus software-saved R4-R11; FPU switching is not implemented.
    inline constexpr std::size_t initial_frame_bytes = 16 * sizeof(stack_word_t);
}
