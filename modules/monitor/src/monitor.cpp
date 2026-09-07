#include <monitor/monitor.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <board_devices.h>
#include <build_config.h>
#include <irq.h>

namespace usart = stm32f411::usart;

static std::array<std::uint8_t, build_config::monitor::rx_buffer_size> rx_buffer {};

template<auto usart>
struct console_t
{
    console_t() noexcept
    {
        usart.configure(board::devices::console_config);
        irq::install<&console_t::irq_handler>(usart, *this);
        irq::enable(usart.irq_number);
    }

    ~console_t() noexcept
    {
        irq::disable(usart.irq_number);
        irq::uninstall(usart.irq_number);
    }

    void irq_handler() const
    {
        const auto sr = usart.status();
        if (usart::sr::ore.decode(sr))
        {
            // Counts hardware overrun events, not the number of lost bytes.
            rx_overrun_events.fetch_add(1, std::memory_order_relaxed);
        }
        // Reading SR followed by DR also clears an overrun, even if RXNE is clear.
        if (usart::sr::rxne.decode(sr) || usart::sr::ore.decode(sr))
        {
            const auto byte = usart.read_data_register();
            const auto write = write_ix.load(std::memory_order_relaxed);
            const auto next = static_cast<std::uint32_t>((write + 1) % rx_buffer.size());
            if (usart::sr::rxne.decode(sr))
            {
                if (next != read_ix.load(std::memory_order_acquire))
                {
                    rx_buffer[write] = byte;
                    write_ix.store(next, std::memory_order_release);
                }
                else
                {
                    rx_dropped_bytes.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    }

    bool try_read(uint8_t& byte) const noexcept
    {
        const auto read = read_ix.load(std::memory_order_relaxed);
        if (read != write_ix.load(std::memory_order_acquire))
        {
            byte = rx_buffer[read];
            const auto next = static_cast<std::uint32_t>((read + 1) % rx_buffer.size());
            read_ix.store(next, std::memory_order_release);
            return true;
        }

        return false;
    }

    std::uint32_t size() const noexcept
    {
        constexpr auto capacity = static_cast<std::uint32_t>(rx_buffer.size());
        // Foreground-consumer snapshot; the ISR may add bytes after this load.
        const auto read = read_ix.load(std::memory_order_relaxed);
        const auto write = write_ix.load(std::memory_order_acquire);
        return (write + capacity - read) % capacity;
    }

    void write(const std::uint8_t data) const noexcept
    {
        while (!usart.is_txe())
            ;
        usart.write_data_register(data);
    }

    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    mutable std::atomic<std::uint32_t> write_ix { 0 };
    mutable std::atomic<std::uint32_t> read_ix { 0 };
    // Debugger diagnostics: inspect console_usart2 after reproducing a paste.
    mutable std::atomic<std::uint32_t> rx_dropped_bytes { 0 };
    mutable std::atomic<std::uint32_t> rx_overrun_events { 0 };
};

using getchar_t = int (*)(void*);
using putchar_t = void (*)(void*, int);

constexpr auto XAM = static_cast<std::uint8_t>(0x00);

struct monitor_workspace
{
    void*                         ctx;
    getchar_t                     getchar;
    putchar_t                     putchar;
    std::array<std::uint8_t, 128> in;
    std::uintptr_t                xam;
    std::uintptr_t                st;
    std::uintptr_t                hex;
    std::uint8_t                  ysav;
    std::uint8_t                  mode;
};

[[gnu::section(".monitor_workspace"), gnu::used]]
monitor_workspace workspace;

static_assert(sizeof(monitor_workspace) <= build_config::monitor::workspace_size);
static_assert(std::is_standard_layout_v<monitor_workspace>);
static_assert(offsetof(monitor_workspace, ctx) == 0);
static_assert(offsetof(monitor_workspace, getchar) == 4);
static_assert(offsetof(monitor_workspace, putchar) == 8);
static_assert(offsetof(monitor_workspace, in) == 12);
static_assert(offsetof(monitor_workspace, xam) == 140);
static_assert(offsetof(monitor_workspace, st) == 144);
static_assert(offsetof(monitor_workspace, hex) == 148);
static_assert(offsetof(monitor_workspace, ysav) == 152);
static_assert(offsetof(monitor_workspace, mode) == 153);

extern "C" void wozmon(monitor_workspace* workspace);

extern "C" void start_wozmon(void* ctx, getchar_t getchar, putchar_t putchar)
{

    workspace = {};
    workspace.ctx = ctx;
    workspace.getchar = getchar;
    workspace.putchar = putchar;
    workspace.mode = XAM;
    wozmon(&workspace);
}

void monitor::run()
{
    board::init_console();
    using console_usart2_t = console_t<board::devices::console>;
    console_usart2_t console_usart2;

    start_wozmon(
        &console_usart2,
        +[](void* ctx) -> int {
            std::uint8_t byte;
            auto*        console = static_cast<console_usart2_t*>(ctx);
            while (!console->try_read(byte))
                ;
            return byte;
        },
        +[](void* ctx, int data) -> void {
            auto* console = static_cast<console_usart2_t*>(ctx);
            console->write(static_cast<std::uint8_t>(data));
        });
}
