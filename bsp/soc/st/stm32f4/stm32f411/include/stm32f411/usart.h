#pragma once
#include <stm32/usart.h>
#include <stm32f411/generated/usart.h>
#include <stm32f411/gpio.h>
#include <stm32f411/rcc.h>

namespace stm32f411::usart {
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
             gpio::alternate_function_t AF,
             artos::irq::irq_t          IRQ,
             typename ClockGate>
    using usart_t = stm32::usart::usart_t<registers_t, RX, TX, Address, AF, IRQ, ClockGate>;
    template<auto RX, auto TX>
    inline constexpr usart_t<RX,
                             TX,
                             usart2_address,
                             gpio::alternate_function_t::AF7,
                             usart2_irq_number,
                             rcc::apb1_gate<rcc::apb1enr::usart2en>>
        usart2;

    template<auto RX, auto TX>
    inline constexpr auto usart2_device = usart2<RX, TX>;
}
