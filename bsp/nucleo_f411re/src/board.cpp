#include <board_devices.h>

void board::init() noexcept
{
    // Retain the reset clock tree. Device clocks are enabled by their consumers.
}

void board::init_console() noexcept
{
    using namespace stm32f411;
    rcc::RCC->AHB1ENR.set(rcc::ahb1enr::gpioaen);
    (void)rcc::RCC->AHB1ENR.read();
    rcc::RCC->APB1ENR.set(rcc::apb1enr::usart2en);
    (void)rcc::RCC->APB1ENR.read();
}
