#pragma once

#include <stm32/usart.h>
#include <stm32f439/generated/usart.h>
#include <stm32f439/gpio.h>
#include <stm32f439/rcc.h>

namespace stm32f439::usart {
    template<auto RX, auto TX>
    inline constexpr stm32::usart::usart_t<usart3::registers_t,
                                           RX,
                                           TX,
                                           usart3::usart3_address,
                                           gpio::alternate_function_t::AF7,
                                           artos::irq::irq_t::usart3,
                                           rcc::apb1_gate<rcc::apb1enr::usart3en>>
        usart3_device {};
}
