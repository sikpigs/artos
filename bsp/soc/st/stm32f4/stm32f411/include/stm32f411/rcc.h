#pragma once

#include <stm32/clock_gate.h>
#include <stm32f411/generated/rcc.h>

namespace stm32f411::rcc {
    template<auto RegisterMember, auto Field>
    using clock_gate = stm32::rcc::clock_gate_t<registers_t, address, RegisterMember, Field>;

    template<auto Field>
    using ahb1_gate = clock_gate<&registers_t::AHB1ENR, Field>;

    template<auto Field>
    using apb1_gate = clock_gate<&registers_t::APB1ENR, Field>;

    template<auto Field>
    using apb2_gate = clock_gate<&registers_t::APB2ENR, Field>;
}
