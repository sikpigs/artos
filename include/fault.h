#pragma once

#include <cstdint>
#include <optional>

namespace fault {
    enum class cause_t : std::uint8_t
    {
        memory_access,
        exception_stack_access,
        exception_unstack_access,
        exception_lazy_stack_access,
        other
    };

    enum class origin_t : std::uint8_t
    {
        thread,
        handler,
        unknown
    };

    enum class stack_type_t : std::uint8_t
    {
        process,
        main,
        unknown,
    };

    struct info_t
    {
        struct address_range_t
        {
            std::uintptr_t begin;
            std::uintptr_t end; // exclusive
        };

        cause_t      cause { cause_t::other };
        origin_t     origin { origin_t::unknown };
        stack_type_t stack_type { stack_type_t::unknown };
        bool         exception_frame_valid { false };

        // The address that caused a mem fault
        std::optional<std::uintptr_t> address {};

        std::optional<address_range_t> exception_stack_access_range {};

        // The SP the interrupted code had immediately before exception entry.
        std::optional<std::uintptr_t> interrupted_stack_pointer {};

        // The exception frame.
        std::uintptr_t exception_frame_address {};
    };

    [[noreturn]] void dispatch(const info_t&) noexcept;
}
