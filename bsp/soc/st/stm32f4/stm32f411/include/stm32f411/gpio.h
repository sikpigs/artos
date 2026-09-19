#pragma once

#include <stm32/gpio.h>
#include <stm32f411/generated/gpio.h>

namespace stm32f411::gpio {
    using stm32::gpio::mode_t;
    using stm32::gpio::output_type_t;
    using stm32::gpio::output_speed_t;
    using stm32::gpio::pull_up_down_t;
    using stm32::gpio::alternate_function_t;
    using stm32::gpio::config_t;

    template<std::uint32_t Pin, std::uintptr_t Address>
    requires(Pin < 16)
    using pin_t = stm32::gpio::pin_t<registers_t, pin_fields<Pin>, Address>;

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpioa_address> PA {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpiob_address> PB {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpioc_address> PC {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpiod_address> PD {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpioe_address> PE {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpioh_address> PH {};
}
