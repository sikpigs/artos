#pragma once

#include <stm32f411/regmap.h>

// Handwritten output contract for the NUCLEO-F411RE board description.
// No initialization occurs by including this header.
namespace board {
    struct root
    {
        struct clocks
        {
            struct hsi
            {
                static constexpr std::uint32_t frequency_hz = 16'000'000;
            };
            // Startup retains the reset clock tree: HSI, AHB /1, APB1 /1.
            using core = hsi;
        };

        struct soc
        {
            struct systick
            {
                using clock = clocks::core;
                static constexpr std::uintptr_t address = 0xE000E010UL;
            };

            struct apb1
            {
                using clock = clocks::core;

                struct serial_40004400
                {
                    static constexpr bool enabled = true;
                    static constexpr auto address = stm32f411::usart::usart2_address;
                    static constexpr auto interrupt = irq::irq_t::usart2;
                    using clock = apb1::clock;
                    static constexpr auto          rx = stm32f411::gpio::PA3;
                    static constexpr auto          tx = stm32f411::gpio::PA2;
                    static constexpr auto          alternate_function = stm32f411::gpio::alternate_function_t::AF7;
                    static constexpr std::uint32_t baud_rate = 115'200;
                };
            };
        };
    };

    namespace labels {
        using uart2 = root::soc::apb1::serial_40004400;
        using systick = root::soc::systick;
    }

    namespace chosen {
        using console = labels::uart2;
        using kernel_tick = labels::systick;
    }

    // Handwritten board bring-up. Call before initializing drivers or ARTOS.
    void init() noexcept;
}
