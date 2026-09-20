#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace artos::thread::internal {
    template<std::unsigned_integral Id, std::size_t Capacity>
    requires(Capacity >= 2
             && static_cast<std::uintmax_t>(Capacity - 1)
                    <= static_cast<std::uintmax_t>(std::numeric_limits<Id>::max()))
    struct thread_id_layout_t
    {
        static constexpr auto index_bits = std::bit_width(static_cast<Id>(Capacity - 1));
        static_assert(index_bits < std::numeric_limits<Id>::digits,
                      "thread IDs must retain at least one generation bit");

        static constexpr Id index_mask = (Id { 1 } << index_bits) - Id { 1 };
        static constexpr Id generation_mask = ~index_mask;
        static constexpr Id generation_increment = Id { 1 } << index_bits;
        static constexpr Id initial_id = generation_mask;

        [[nodiscard]] static constexpr Id next(Id current, std::size_t index) noexcept
        {
            const auto generation = Id { (current & generation_mask) + generation_increment };
            return Id { (generation & generation_mask) | (static_cast<Id>(index) & index_mask) };
        }
    };
}
