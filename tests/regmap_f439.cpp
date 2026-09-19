#include <stm32f439/gpio.h>
#include <stm32f439/regmap.h>
static_assert(stm32f439::gpio::gpiod_address == 0x40020C00);
static_assert(stm32f439::gpio::PD<9>.number == 9);
static_assert(stm32f439::gpio::pin_fields<9>::mode.mask == stm32f439::gpio::moder::moder9.mask);
static_assert(stm32f439::gpio::pin_fields<9>::alternate_function.mask == stm32f439::gpio::afrh::afrh9.mask);
static_assert(offsetof(stm32f439::usart::usart3::registers_t, CR1) == 0x0C);
static_assert(stm32f439::usart::usart3::usart3_address == 0x40004800);
