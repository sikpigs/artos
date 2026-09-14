#include <board_devices.h>

void board::init() noexcept
{
    devices::enable_heartbeat_clock();
    devices::heartbeat.write(false);
    devices::heartbeat.configure({ .port_mode = stm32::gpio::mode_t::output });
}

void board::init_console() noexcept
{
    devices::enable_console_clocks();
}
