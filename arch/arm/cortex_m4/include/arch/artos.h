#pragma once

#include <cstddef>
#include <cstdint>

namespace artos::port {
    using stack_word_t = std::uint32_t;
    using tick_t = stack_word_t;
    inline constexpr std::size_t stack_alignment = 8;

    struct stack_layout_requirements_t
    {
        std::size_t usable_bottom_alignment;
        std::size_t reserved_prefix_bytes;
        std::size_t usable_size_alignment;
    };

    [[nodiscard]] constexpr stack_layout_requirements_t stack_layout_requirements() noexcept
    {
        return {
            .usable_bottom_alignment = 32,
            .reserved_prefix_bytes = 32,
            .usable_size_alignment = stack_alignment,
        };
    }

    // New threads start with a basic exception frame plus R4-R11/EXC_RETURN.
    // This is the initial context size, not the minimum usable thread stack.
    inline constexpr std::size_t  initial_frame_bytes = 17 * sizeof(stack_word_t);
    inline constexpr stack_word_t initial_exc_return = 0xFFFFFFFDu;
}
