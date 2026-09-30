#pragma once

#include <stm32/timer.h>
#include <stm32f411/generated/tim.h>
#include <stm32f411/rcc.h>

namespace stm32f411::timer {
    using stm32::timer::config_t;
    using stm32::timer::timer_t;

    inline constexpr timer_t<tim::tim1::registers_t,
                             tim::tim1::tim1_address,
                             16,
                             0x3FFU,
                             rcc::apb2_gate<rcc::apb2enr::tim1en>>
        timer1 {};

    inline constexpr timer_t<tim::tim2::registers_t,
                             tim::tim2::tim2_address,
                             32,
                             0x3FFU,
                             rcc::apb1_gate<rcc::apb1enr::tim2en>>
        timer2 {};

    inline constexpr timer_t<tim::tim3::registers_t,
                             tim::tim3::tim3_address,
                             16,
                             0x3FFU,
                             rcc::apb1_gate<rcc::apb1enr::tim3en>>
        timer3 {};

    inline constexpr timer_t<tim::tim4::registers_t,
                             tim::tim4::tim4_address,
                             16,
                             0x3FFU,
                             rcc::apb1_gate<rcc::apb1enr::tim4en>>
        timer4 {};

    inline constexpr timer_t<tim::tim5::registers_t,
                             tim::tim5::tim5_address,
                             32,
                             0x3FFU,
                             rcc::apb1_gate<rcc::apb1enr::tim5en>>
        timer5 {};

    inline constexpr timer_t<tim::tim9::registers_t,
                             tim::tim9::tim9_address,
                             16,
                             0x38FU,
                             rcc::apb2_gate<rcc::apb2enr::tim9en>>
        timer9 {};

    inline constexpr timer_t<tim::tim10::registers_t,
                             tim::tim10::tim10_address,
                             16,
                             0x387U,
                             rcc::apb2_gate<rcc::apb2enr::tim10en>>
        timer10 {};

    inline constexpr timer_t<tim::tim11::registers_t,
                             tim::tim11::tim11_address,
                             16,
                             0x387U,
                             rcc::apb2_gate<rcc::apb2enr::tim11en>>
        timer11 {};
}
