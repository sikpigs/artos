#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <ratio>

namespace artos {
    template<std::unsigned_integral Rep, typename Scale = std::ratio<1>>
    struct frequency_t
    {
        static_assert(Scale::num > 0 && Scale::den > 0);

        Rep value;

        [[nodiscard]] constexpr Rep count() const noexcept
        {
            return value;
        }

        [[nodiscard]] constexpr bool operator==(const frequency_t&) const noexcept = default;
    };

    using hertz_t = frequency_t<std::uint32_t>;
    using kilohertz_t = frequency_t<std::uint32_t, std::kilo>;
    using megahertz_t = frequency_t<std::uint32_t, std::mega>;

    // An inexact or overflowing conversion must be handled by the caller.
    template<std::unsigned_integral Rep, typename Scale>
    [[nodiscard]] constexpr std::optional<hertz_t> to_hertz(frequency_t<Rep, Scale> frequency) noexcept
    {
        constexpr auto numerator = static_cast<std::uint64_t>(Scale::num);
        constexpr auto denominator = static_cast<std::uint64_t>(Scale::den);
        const auto     count = static_cast<std::uint64_t>(frequency.count());
        if (count > std::numeric_limits<std::uint64_t>::max() / numerator)
        {
            return std::nullopt;
        }
        const auto scaled = count * numerator;
        const auto hertz = scaled / denominator;
        if (scaled % denominator != 0 || hertz > std::numeric_limits<std::uint32_t>::max())
        {
            return std::nullopt;
        }
        return hertz_t { static_cast<std::uint32_t>(hertz) };
    }
}
