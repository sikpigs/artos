#pragma once

#include <cstddef>
#include <limits>

namespace artos::deferred {
    using deferred_callback_t = void (*)(void*) noexcept;

    struct handle_t
    {
        std::size_t index;
    };

    inline constexpr handle_t invalid_deferred_handle { .index = std::numeric_limits<std::size_t>::max() };

    handle_t register_deferred(deferred_callback_t callback, void* context) noexcept;

    void pend_deferred(const handle_t& handle) noexcept;
}