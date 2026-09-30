#pragma once

#include <stm32/clock_gate.h>
#include <stm32f439/generated/rcc.h>

namespace stm32f439::rcc {
    // The supplied SVD omits the GPIOJ/K enable fields, which occupy bits 9/10.
    namespace ahb1enr {
        inline constexpr artos::mmio::field_t<tags::ahb1enr, 9>  gpiojen {};
        inline constexpr artos::mmio::field_t<tags::ahb1enr, 10> gpioken {};
    }

    template<auto RegisterMember, auto Field>
    using clock_gate = stm32::rcc::clock_gate_t<registers_t, address, RegisterMember, Field>;

    template<auto Field>
    using ahb1_gate = clock_gate<&registers_t::AHB1ENR, Field>;

    template<auto Field>
    using apb1_gate = clock_gate<&registers_t::APB1ENR, Field>;

    template<auto Field>
    using apb2_gate = clock_gate<&registers_t::APB2ENR, Field>;
}
