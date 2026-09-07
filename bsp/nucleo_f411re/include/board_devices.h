#pragma once

#include <board.h>

namespace board {
    void init_console() noexcept;
}

// Handwritten adapters from description nodes to existing peripheral drivers.
namespace board::devices {
    using console_node = chosen::console;
    static_assert(console_node::enabled);
    inline constexpr stm32f411::usart::usart_t<console_node::rx,
                                               console_node::tx,
                                               console_node::address,
                                               console_node::clock::frequency_hz,
                                               console_node::alternate_function,
                                               console_node::interrupt>
        console {};

    inline constexpr stm32f411::usart::config_t console_config {
        .baud_rate = console_node::baud_rate,
        .rx_dr_not_empty_intr_en = true,
    };
    static_assert(stm32f411::usart::calculate_brr(console_node::clock::frequency_hz,
                                                  console_config.baud_rate,
                                                  console_config.sampling)
                  != 0);
}
