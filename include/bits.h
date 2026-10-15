#pragma once

#include <concepts>
#include <cstddef>
#include <limits>

template<std::integral T, std::size_t Bit>
requires(Bit < std::numeric_limits<T>::digits)
inline constexpr T bit = static_cast<T>(T { 1 } << Bit);

template<std::integral T, std::size_t... Bits>
requires((Bits < std::numeric_limits<T>::digits) && ...)
inline constexpr T bits = (static_cast<T>(T { 1 } << Bits) | ...);
