#pragma once

#include <stm32/gpio.h>
#include <stm32f411/generated/gpio.h>

// Handwritten GPIO configuration and pin operations.
namespace stm32f411::gpio {
    namespace moder {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr artos::mmio::field_t<tags::moder, Pin * 2, 2> mode {};
    }

    namespace otyper {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr artos::mmio::field_t<tags::otyper, Pin> type {};
    }

    namespace ospeedr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr artos::mmio::field_t<tags::ospeedr, Pin * 2, 2> speed {};
    }

    namespace pupdr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr artos::mmio::field_t<tags::pupdr, Pin * 2, 2> pull {};
    }

    namespace idr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr artos::mmio::field_t<tags::idr, Pin> input {};
    }

    namespace odr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr artos::mmio::field_t<tags::odr, Pin> output {};
    }

    namespace bsrr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr artos::mmio::field_t<tags::bsrr, Pin> set {};

        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr artos::mmio::field_t<tags::bsrr, Pin + 16> reset {};
    }

    namespace lckr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr artos::mmio::field_t<tags::lckr, Pin> lock {};

        inline constexpr artos::mmio::field_t<tags::lckr, 16> key {};
    }

    namespace afrl {
        template<std::uint32_t Pin>
        requires(Pin < 8)
        inline constexpr artos::mmio::field_t<tags::afrl, Pin * 4, 4> alternate_function {};
    }

    namespace afrh {
        template<std::uint32_t Pin>
        requires(Pin >= 8 && Pin < 16)
        inline constexpr artos::mmio::field_t<tags::afrh, (Pin - 8) * 4, 4> alternate_function {};
    }

    using stm32::gpio::mode_t;
    using stm32::gpio::output_type_t;
    using stm32::gpio::output_speed_t;
    using stm32::gpio::pull_up_down_t;
    using stm32::gpio::alternate_function_t;
    using stm32::gpio::config_t;
    template<std::uint32_t Pin, std::uintptr_t Address>
    using pin_t = stm32::gpio::pin_t<registers_t, Pin, Address>;

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

    inline constexpr auto PA0 = PA<0>;
    inline constexpr auto PA1 = PA<1>;
    inline constexpr auto PA2 = PA<2>;
    inline constexpr auto PA3 = PA<3>;
    inline constexpr auto PA4 = PA<4>;
    inline constexpr auto PA5 = PA<5>;
}
