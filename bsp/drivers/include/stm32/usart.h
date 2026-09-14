#pragma once

#include <cstddef>
#include <irq.h>
#include <mmio.h>
#include <stm32/gpio.h>
#include <utility>

// Handwritten USART configuration, baud calculation, and device operations.
namespace stm32::usart {
    inline constexpr std::uint32_t fpclk = 16'000'000;

    enum class oversampling_t : std::uint8_t
    {
        by_16,
        by_8,
    };

    [[nodiscard]] constexpr std::uint32_t calculate_brr(std::uint32_t  peripheral_clock_hz,
                                                        std::uint32_t  baud_rate,
                                                        oversampling_t mode = oversampling_t::by_16) noexcept
    {
        if (baud_rate == 0)
        {
            return 0;
        }

        const auto divider
            = static_cast<std::uint32_t>((static_cast<std::uint64_t>(peripheral_clock_hz) + baud_rate / 2) / baud_rate);

        if (mode == oversampling_t::by_8)
        {
            if (divider < 8 || divider > 0x7FFF)
            {
                return 0;
            }

            // In OVER8 mode BRR[3] must remain clear; the three fraction
            // bits retain their position while the mantissa shifts left.
            return ((divider & ~0x7UL) << 1) | (divider & 0x7UL);
        }

        return divider >= 16 && divider <= 0xFFFF ? divider : 0;
    }

    [[nodiscard]] constexpr std::uint32_t calculate_brr(std::uint32_t  baud_rate,
                                                        oversampling_t mode = oversampling_t::by_16) noexcept
    {
        return calculate_brr(fpclk, baud_rate, mode);
    }

    static_assert(calculate_brr(16'000'000, 115'200) == 0x008B);
    static_assert(calculate_brr(115'200) == 0x008B);
    static_assert(calculate_brr(16'000'000, 115'200, oversampling_t::by_8) == 0x0113);

    enum class word_len_t : std::uint8_t
    {
        bits_8,
        bits_9
    };

    enum class parity_t : std::uint8_t
    {
        even,
        odd
    };

    enum class parity_control_t : std::uint8_t
    {
        disabled,
        enabled
    };

    enum class wakeup_method_t : std::uint8_t
    {
        idle,
        address
    };

    struct config_t
    {
        std::uint32_t    baud_rate {};
        bool             rx_en { true };
        bool             tx_en { true };
        bool             idle_intr_en { false };
        bool             rx_dr_not_empty_intr_en { false };
        bool             tx_cplt_intr_en { false };
        bool             tx_dr_empty_intr_en { false };
        bool             parity_error_intr_en { false };
        bool             enable { true };
        parity_t         parity_select { parity_t::even };
        parity_control_t parity_control_en { parity_control_t::disabled };
        wakeup_method_t  wakeup { wakeup_method_t::idle };
        word_len_t       length { word_len_t::bits_8 };
        oversampling_t   sampling { oversampling_t::by_16 };

        [[nodiscard]] constexpr std::uint32_t cr1() const noexcept
        {
            return (std::uint32_t(rx_en) << 2) | (std::uint32_t(tx_en) << 3) | (std::uint32_t(idle_intr_en) << 4)
                   | (std::uint32_t(rx_dr_not_empty_intr_en) << 5) | (std::uint32_t(tx_cplt_intr_en) << 6)
                   | (std::uint32_t(tx_dr_empty_intr_en) << 7) | (std::uint32_t(parity_error_intr_en) << 8)
                   | (std::uint32_t(enable) << 13) | (std::uint32_t(std::to_underlying(parity_select)) << 9)
                   | (std::uint32_t(std::to_underlying(parity_control_en)) << 10)
                   | (std::uint32_t(std::to_underlying(wakeup)) << 11)
                   | (std::uint32_t(std::to_underlying(length)) << 12)
                   | (std::uint32_t(std::to_underlying(sampling)) << 15);
        }
    };

    template<typename Registers,
             auto                       RX,
             auto                       TX,
             std::uintptr_t             Address,
             std::uint32_t              FpClk,
             gpio::alternate_function_t AltFunc,
             artos::irq::irq_t          IRQNumber>
    struct usart_t
    {
        static_assert(offsetof(Registers, SR) == 0x00);
        static_assert(offsetof(Registers, DR) == 0x04);
        static_assert(offsetof(Registers, BRR) == 0x08);
        static_assert(offsetof(Registers, CR1) == 0x0C);

    private:
        Registers* usart() const noexcept
        {
            return reinterpret_cast<Registers*>(Address);
        }

    public:
        static constexpr auto               irq_number = IRQNumber;
        [[nodiscard]] static constexpr bool has_overrun(std::uint32_t status) noexcept
        {
            return (status & (1U << 3)) != 0;
        }
        [[nodiscard]] static constexpr bool has_rx_data(std::uint32_t status) noexcept
        {
            return (status & (1U << 5)) != 0;
        }

        [[nodiscard]] bool is_cts() const noexcept
        {
            return (usart()->SR.read() & (1U << 9)) != 0;
        }

        [[nodiscard]] std::uint32_t status() const noexcept
        {
            return usart()->SR.read();
        }

        [[nodiscard]] std::uint8_t read_data_register() const noexcept
        {
            return static_cast<std::uint8_t>(usart()->DR.read());
        }

        void write_data_register(const std::uint8_t value) const noexcept
        {
            // DR reads consume received data. Use the whole-register overload
            // so transmitting never performs a read/modify/write on RX data.
            usart()->DR.write(value);
        }

        [[nodiscard]] bool is_txe() const noexcept
        {
            return (usart()->SR.read() & (1U << 7)) != 0;
        }

        void configure(const config_t& cfg) const noexcept
        {
            usart()->CR1.clear(artos::mmio::field_t<typename decltype(Registers::CR1)::tag_type, 13> {});

            constexpr gpio::config_t pin_cfg = { .port_mode = gpio::mode_t::alt_func, .alt_func = AltFunc };
            RX.configure(pin_cfg);
            TX.configure(pin_cfg);

            usart()->BRR.write(calculate_brr(FpClk, cfg.baud_rate, cfg.sampling));
            usart()->CR1.write(cfg.cr1());
        }
    };

}
