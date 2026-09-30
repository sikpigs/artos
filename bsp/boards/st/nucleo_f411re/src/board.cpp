#include <board_devices.h>

void board::init() noexcept
{
    devices::heartbeat.configure({ .port_mode = stm32::gpio::mode_t::output });
    devices::heartbeat.write(false);
}
