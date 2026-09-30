#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include <frequency.h>
#include <mmio.h>

namespace stm32::timer {

    // CKD selects the digital-filter clock; the counter rate is set by PSC.
    enum class clock_division_t : std::uint8_t
    {
        clk_int_x1 = 0,
        clk_int_x2 = 1,
        clk_int_x4 = 2
    };

    enum class auto_reload_preload_enable_t : std::uint8_t
    {
        not_buffered = 0,
        buffered = 1,
    };

    enum class center_aligned_t : std::uint8_t
    {
        edge_aligned = 0,
        center_aligned_compare_on_downcount = 1,
        center_aligned_compare_on_upcount = 2,
        center_aligned_compare_on_both = 3,
    };

    enum class direction_t : std::uint8_t
    {
        up = 0,
        down = 1
    };

    enum class one_pulse_mode_t : std::uint8_t
    {
        free_running = 0,
        one_pulse = 1
    };

    enum class update_request_source_t : std::uint8_t
    {
        any_update_event = 0,
        counter_overflow_or_underflow = 1,
    };

    enum class update_event_t : std::uint8_t
    {
        enabled = 0,
        disabled = 1
    };

    struct config_t
    {
        clock_division_t             clock_division { clock_division_t::clk_int_x1 };
        auto_reload_preload_enable_t auto_reload_preload_enable { auto_reload_preload_enable_t::not_buffered };
        center_aligned_t             center_aligned { center_aligned_t::edge_aligned };
        direction_t                  direction { direction_t::up };
        one_pulse_mode_t             one_pulse_mode { one_pulse_mode_t::free_running };
        update_request_source_t      update_request_source { update_request_source_t::any_update_event };
        update_event_t               update_event { update_event_t::enabled };
        bool                         update_interrupt_enable { false };
        // No requested frequency selects the fastest counter rate (PSC = 0).
        std::optional<artos::hertz_t> tick_frequency {};
        std::uint32_t                 reload { 0 };
    };

    // PSC + 1 is the divisor. Choose the divisor with the closest output rate.
    [[nodiscard]] constexpr std::optional<std::uint32_t> calculate_prescaler(artos::hertz_t timer_clock,
                                                                             artos::hertz_t tick_frequency) noexcept
    {
        const auto timer_clock_hz = timer_clock.count();
        const auto tick_frequency_hz = tick_frequency.count();
        if (timer_clock_hz == 0 || tick_frequency_hz == 0)
        {
            return std::nullopt;
        }
        if (tick_frequency_hz >= timer_clock_hz)
        {
            return 0;
        }
        const auto lower = timer_clock_hz / tick_frequency_hz;
        if (lower >= 65'536)
        {
            return 65'535;
        }
        const auto upper = lower + 1;
        const auto lower_error
            = (static_cast<std::uint64_t>(timer_clock_hz) - static_cast<std::uint64_t>(tick_frequency_hz) * lower)
              * upper;
        const auto upper_error = (static_cast<std::uint64_t>(tick_frequency_hz) * upper - timer_clock_hz) * lower;
        return lower_error <= upper_error ? lower - 1 : upper - 1;
    }

    template<typename Registers,
             std::uintptr_t Address,
             std::uint32_t  CounterBits,
             std::uint32_t  SupportedCr1Mask,
             typename ClockGate>
    struct timer_t
    {
        static_assert(CounterBits == 16 || CounterBits == 32);
        static_assert((SupportedCr1Mask & 1U) != 0);
        static_assert(offsetof(Registers, CR1) == 0x00);
        static_assert(offsetof(Registers, DIER) == 0x0C);
        static_assert(offsetof(Registers, SR) == 0x10);
        static_assert(offsetof(Registers, EGR) == 0x14);
        static_assert(offsetof(Registers, CNT) == 0x24);
        static_assert(offsetof(Registers, PSC) == 0x28);
        static_assert(offsetof(Registers, ARR) == 0x2C);

        static constexpr auto          address = Address;
        static constexpr std::uint32_t max_reload = CounterBits == 32 ? 0xFFFF'FFFFU : 0xFFFFU;

    private:
        static Registers* tim() noexcept
        {
            return reinterpret_cast<Registers*>(Address);
        }

        [[nodiscard]] static constexpr std::uint32_t cr1_value(const config_t& cfg) noexcept
        {
            const auto bits = [](auto value) { return static_cast<std::uint32_t>(std::to_underlying(value)); };
            return (bits(cfg.update_event) << 1) | (bits(cfg.update_request_source) << 2)
                   | (bits(cfg.one_pulse_mode) << 3) | (bits(cfg.direction) << 4) | (bits(cfg.center_aligned) << 5)
                   | (bits(cfg.auto_reload_preload_enable) << 7) | (bits(cfg.clock_division) << 8);
        }

    public:
        [[nodiscard]] static constexpr config_t free_running_up_counter() noexcept
        {
            config_t cfg {};
            cfg.reload = max_reload;
            return cfg;
        }

        // Returns false without changing registers for invalid clock, reload, or CR1 options.
        // Enable the peripheral clock after validation and before register access.
        [[nodiscard]] bool configure(const config_t& cfg, artos::hertz_t timer_clock) const noexcept
        {
            const auto control = cr1_value(cfg);
            if (timer_clock.count() == 0 || cfg.reload == 0 || cfg.reload > max_reload
                || (control & ~SupportedCr1Mask) != 0)
            {
                return false;
            }
            std::uint32_t prescaler = 0;
            if (cfg.tick_frequency)
            {
                const auto requested_prescaler = calculate_prescaler(timer_clock, *cfg.tick_frequency);
                if (!requested_prescaler)
                {
                    return false;
                }
                prescaler = *requested_prescaler;
            }

            ClockGate::enable();

            auto* registers = tim();
            registers->DIER.write(0);            // No interrupt from the software update below.
            registers->CR1.write(control & ~2U); // CEN and UDIS clear while applying the configuration.
            if constexpr (requires { registers->SMCR; })
            {
                registers->SMCR.write(0); // Internal clock, independent of the prior slave mode.
            }
            if constexpr (requires { registers->RCR; })
            {
                registers->RCR.write(0);
            }
            registers->PSC.write(prescaler);
            registers->ARR.write(cfg.reload);
            registers->CNT.write(0);
            // Transfer the buffered prescaler and reload before the first start.
            // UDIS must be clear for UG to take effect, so set it afterwards.
            registers->EGR.write(1U);
            registers->SR.write(~1U); // UIF is rc_w0; preserve other pending flags.
            registers->DIER.write(cfg.update_interrupt_enable ? 1U : 0U);
            registers->CR1.write(control);
            return true;
        }

        void start() const noexcept
        {
            tim()->CR1.set(artos::mmio::field_t<typename decltype(Registers::CR1)::tag_type, 0> {});
        }

        void stop() const noexcept
        {
            tim()->CR1.clear(artos::mmio::field_t<typename decltype(Registers::CR1)::tag_type, 0> {});
        }

        [[nodiscard]] std::uint32_t count() const noexcept
        {
            return tim()->CNT.read();
        }

        [[nodiscard]] bool rollover() const noexcept
        {
            auto* registers = tim();
            if ((registers->SR.read() & 1U) == 0)
            {
                return false;
            }

            registers->SR.write(~1U);
            return true;
        }
    };
}
