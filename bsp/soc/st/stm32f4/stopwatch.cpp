#include <board_services.h>

#include <board_devices.h>
#include <cpu_port.h>
#include <stm32/timer.h>

void board::stopwatch::init() noexcept
{
    auto config = devices::stopwatch.free_running_up_counter();
    config.update_request_source = stm32::timer::update_request_source_t::counter_overflow_or_underflow;
    if (!devices::stopwatch.configure(config, devices::stopwatch_clock_hz))
    {
        cpu::port::fatal_trap();
    }
    devices::stopwatch.start();
}

std::uint32_t board::stopwatch::count() noexcept
{
    return devices::stopwatch.count();
}

std::uint32_t board::stopwatch::clock_hz() noexcept
{
    return devices::stopwatch_clock_hz.count();
}
