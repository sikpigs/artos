#include <board_devices.h>

#include <artos.h>
#include <deferred.h>
#include <irq.h>
#include <port/cpu_port.h>

#include <console/console.h>

namespace artos::console {
    namespace {
        constexpr auto     usart = board::devices::console;
        buffered_console_t console;
    }

    buffered_console_t& system_console() noexcept
    {
        return console;
    }

    void buffered_console_t::init() noexcept
    {
        // Startup only, before clients run. The instance lives for the system lifetime.
        if (initialized)
        {
            return;
        }

        deferred_handle = artos::deferred::register_deferred(
            +[](void* context) noexcept {
                auto* buffered_console = reinterpret_cast<buffered_console_t*>(context);
                buffered_console->deferred_irq_handler();
            },
            this);

        if (deferred_handle.index == deferred::invalid_deferred_handle.index)
        {
            cpu::port::fatal_trap();
        }

        usart.configure(board::devices::console_config, board::devices::console_clock_hz);
        artos::irq::install<&buffered_console_t::irq_handler>(usart, *this);

        initialized = true;
        artos::irq::enable(usart.irq_number);
    }

    void buffered_console_t::deferred_irq_handler() noexcept
    {
        const critical_section_t cs {};
        if (!empty())
        {
            artos::kernel::locked::wake_one(rx_wait_queue);
        }
    }

    void buffered_console_t::irq_handler() noexcept
    {
        const auto sr = usart.status();
        if (usart.has_overrun(sr))
        {
            // Counts hardware overrun events, not the number of lost bytes.
            rx_overrun_events.fetch_add(1, std::memory_order_relaxed);
        }
        // Reading SR followed by DR also clears an overrun, even if RXNE is clear.
        if (usart.has_rx_data(sr) || usart.has_overrun(sr))
        {
            const auto byte = usart.read_data_register();
            const auto write = write_ix.load(std::memory_order_relaxed);
            const auto next = static_cast<std::uint32_t>((write + 1) % rx_buffer.size());
            if (usart.has_rx_data(sr))
            {
                if (next != read_ix.load(std::memory_order_acquire))
                {
                    rx_buffer[write] = byte;
                    write_ix.store(next, std::memory_order_release);
                    artos::deferred::pend_deferred(deferred_handle);
                }
                else
                {
                    rx_dropped_bytes.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    }

    std::uint8_t buffered_console_t::read_byte() noexcept
    {
        std::uint8_t byte;

        while (true)
        {
            if (try_read(byte))
            {
                return byte;
            }

            block_if_empty();
        }
    }

    bool buffered_console_t::try_read(std::uint8_t& byte) noexcept
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

    void buffered_console_t::write(std::span<const char> sp) noexcept
    {
        for (auto ch : sp)
        {
            write(ch);
        }
    }

    std::uint32_t buffered_console_t::size() noexcept
    {
        constexpr auto capacity = static_cast<std::uint32_t>(build_config::console::rx_buffer_size);
        // Foreground-consumer snapshot; the ISR may add bytes after this load.
        const auto read = read_ix.load(std::memory_order_relaxed);
        const auto write = write_ix.load(std::memory_order_acquire);
        return (write + capacity - read) % capacity;
    }

    void buffered_console_t::write(const std::uint8_t data) noexcept
    {
        while (!usart.is_txe())
            ;
        usart.write_data_register(data);
    }

    bool buffered_console_t::empty() const noexcept
    {
        const auto read = read_ix.load(std::memory_order_relaxed);
        return read == write_ix.load(std::memory_order_acquire);
    }

    void buffered_console_t::block_if_empty() noexcept
    {
        critical_section_t cs {};

        if (!empty())
        {
            return;
        }

        (void)kernel::locked::block_current(rx_wait_queue, artos::wait_forever, std::move(cs));
    }
}
