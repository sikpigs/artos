#include <board_devices.h>
#include <build_config.h>
#include <irq.h>
#include <mcu.h>

static_assert(mcu::readable_ram(mcu::ram_start, mcu::ram_size));
static_assert(!mcu::readable_ram(mcu::ram_start + mcu::ram_size - 16, 32));
static_assert(!mcu::readable_ram(0xFFFFFFFC, 32));
static_assert(board::devices::console.address == board::chosen::console::address);
static_assert(board::devices::console.irq_number == board::chosen::console::interrupt);
static_assert(board::devices::console.alternate_function == board::chosen::console::alternate_function);
static_assert(board::chosen::stopwatch::counter_width == 32);
static_assert(board::chosen::stopwatch::interrupt == artos::irq::irq_t::tim2);
static_assert(board::chosen::stopwatch::address == 0x40000000);
static_assert(board::devices::stopwatch.address == board::chosen::stopwatch::address);
static_assert(board::devices::stopwatch_clock_hz == artos::hertz_t { 16'000'000 });
static_assert(artos::irq::is_valid(board::chosen::console::interrupt));
static_assert(board::chosen::console::clock::frequency_hz == 16'000'000);
static_assert(stm32::usart::calculate_brr(board::devices::console_clock_hz, 115200) == 0x8B);
#ifdef CONFIG_BOARD_NUCLEO_F411RE
static_assert(mcu::flash_size == 512 * 1024);
static_assert(mcu::ram_size == 128 * 1024);
static_assert(mcu::ccm_size == 0);
static_assert(board::chosen::console::address == 0x40004400);
static_assert(board::chosen::console::tx.address == 0x40020000);
static_assert(board::chosen::console::tx.number == 2);
static_assert(board::chosen::console::rx.number == 3);
static_assert(static_cast<unsigned>(board::chosen::console::interrupt) == 38);
static_assert(board::devices::heartbeat.number == 5);
#else
static_assert(mcu::flash_size == 2048 * 1024);
static_assert(mcu::ram_size == 192 * 1024);
static_assert(mcu::ccm_size == 64 * 1024);
static_assert(mcu::readable_ram(mcu::ccm_start, mcu::ccm_size));
static_assert(board::chosen::console::address == 0x40004800);
static_assert(board::chosen::console::tx.address == 0x40020C00);
static_assert(board::chosen::console::tx.number == 8);
static_assert(board::chosen::console::rx.number == 9);
static_assert(static_cast<unsigned>(board::chosen::console::interrupt) == 39);
static_assert(board::devices::heartbeat.number == 0);
static_assert(board::devices::heartbeat.address == 0x40020400);
#endif
