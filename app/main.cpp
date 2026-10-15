#include <cstdint>

#include <board_devices.h>
#include <build_config.h>
#include <port/cpu_port.h>

#ifdef CONFIG_ARTOS
#include <artos.h>
#endif
#ifdef CONFIG_CONSOLE
#include <console/console.h>
#endif
#ifdef CONFIG_MONITOR
#include <monitor/monitor.h>
#endif

#if defined(CONFIG_ARTOS) && defined(CONFIG_MONITOR)
namespace {
    // Startup resets MSP: task storage must outlive main's boot stack.
    artos::thread::storage_t<4096> monitor_storage {};
    // Includes nested sleep/scheduler calls and the saved exception context.
    // A 256-byte stack overflows into the TCB in the unoptimized Debug build.
    artos::thread::storage_t<artos::thread::minimum_stack_bytes> blinky_storage {};

    void monitor_entry(void*)
    {
        monitor::run();
        cpu::port::fatal_trap(); // The monitor is expected to run indefinitely.
    }

    std::uint32_t get_the_ultimate_answer() noexcept
    {
        // SVC writes r0 through the assembly output operand.
        // NOLINTNEXTLINE(misc-const-correctness)
        register std::uint32_t result __asm__("r0") = 0;

        __asm__ volatile("svc #0" : "+r"(result) : : "r1", "r2", "r3", "r12", "cc", "memory");

        return result;
    }

    void blinky_entry(void*)
    {
        artos::tick_t ticks_to_wait = 0;
        if (get_the_ultimate_answer() != 42)
        {
            ticks_to_wait = artos::kernel::ms_to_ticks(500);
        }
        else
        {
            ticks_to_wait = artos::kernel::ms_to_ticks(1000);
        }

        while (true)
        {
            board::devices::heartbeat.write(!board::devices::heartbeat.read_output());
            artos::thread::sleep_for(ticks_to_wait);
        }
    }
}

#endif

extern "C" void app_main() noexcept
{
#if defined(CONFIG_ARTOS) && defined(CONFIG_MONITOR)
    const auto blinky_task
        = artos::thread::create(blinky_entry, "blinky", nullptr, artos::thread::priority_t::low, blinky_storage);
    if (!blinky_task)
    {
        cpu::port::fatal_trap();
    }

    const auto monitor_task
        = artos::thread::create(monitor_entry, "monitor", nullptr, artos::thread::priority_t::low, monitor_storage);
    if (!monitor_task)
    {
        cpu::port::fatal_trap();
    }
#endif
}
