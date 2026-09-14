#pragma once

#include <build_config.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <deferred.h>
#include <span>

#include <wait.h>

namespace artos::console {
    // One ISR producer and one thread consumer. Initialize before use.
    class buffered_console_t
    {
    public:
        buffered_console_t() noexcept = default;
        buffered_console_t(const buffered_console_t&) = delete;
        buffered_console_t& operator=(const buffered_console_t&) = delete;

        void          init() noexcept;
        bool          try_read(std::uint8_t& byte) noexcept;
        std::uint32_t size() noexcept;
        void          write(std::uint8_t byte) noexcept;
        void          write(std::span<const char> bytes) noexcept;
        std::uint8_t  read_byte() noexcept;

    private:
        using rx_buffer_t = std::array<std::uint8_t, build_config::console::rx_buffer_size>;

        bool                       initialized {};
        rx_buffer_t                rx_buffer {};
        wait_queue_t               rx_wait_queue {};
        deferred::handle_t         deferred_handle { deferred::invalid_deferred_handle };
        std::atomic<std::uint32_t> write_ix { 0 };
        std::atomic<std::uint32_t> read_ix { 0 };
        std::atomic<std::uint32_t> rx_dropped_bytes { 0 };
        std::atomic<std::uint32_t> rx_overrun_events { 0 };

        static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

        bool empty() const noexcept;
        void irq_handler() noexcept;
        void block_if_empty() noexcept;
        void deferred_irq_handler() noexcept;
    };

    // Statically allocated system console, bound to the DTS chosen UART.
    buffered_console_t& system_console() noexcept;
}
