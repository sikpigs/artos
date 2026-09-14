#pragma once
#include <stm32/usart.h>
#include <stm32f411/generated/usart.h>
#include <stm32f411/gpio.h>
namespace stm32f411::usart {
    using stm32::usart::fpclk;
    using stm32::usart::oversampling_t;
    using stm32::usart::calculate_brr;
    using stm32::usart::word_len_t;
    using stm32::usart::parity_t;
    using stm32::usart::parity_control_t;
    using stm32::usart::wakeup_method_t;
    using stm32::usart::config_t;
    inline constexpr auto usart2_irq_number = artos::irq::irq_t::usart2;
    template<auto                       RX,
             auto                       TX,
             std::uintptr_t             Address,
             std::uint32_t              Clock,
             gpio::alternate_function_t AF,
             artos::irq::irq_t          IRQ>
    using usart_t = stm32::usart::usart_t<registers_t, RX, TX, Address, Clock, AF, IRQ>;
    template<auto RX, auto TX>
    inline constexpr usart_t<RX, TX, usart2_address, fpclk, gpio::alternate_function_t::AF7, usart2_irq_number> usart2;
}
