#pragma once

#include <cstdint>

namespace stm32::rcc {
    template<typename Registers, std::uintptr_t Address, auto RegisterMember, auto Field>
    struct clock_gate_t
    {
        static void enable() noexcept
        {
            auto& enable_register = reinterpret_cast<Registers*>(Address)->*RegisterMember;
            enable_register.set(Field);
            (void)enable_register.read();
        }
    };
}
