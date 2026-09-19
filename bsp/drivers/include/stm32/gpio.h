#pragma once

#include <cstdint>
#include <utility>

// STM32 GPIO peripheral with MODER/AFR registers (F4-compatible layout).
namespace stm32::gpio {
    enum class mode_t : std::uint8_t
    {
        input = 0,
        output = 1,
        alt_func = 2,
        analog = 3
    };

    enum class output_type_t : std::uint8_t
    {
        push_pull = 0,
        open_drain = 1
    };

    enum class output_speed_t : std::uint8_t
    {
        low = 0,
        medium = 1,
        fast = 2,
        high = 3
    };

    enum class pull_up_down_t : std::uint8_t
    {
        no_pull_up_down = 0,
        pull_up = 1,
        pull_down = 2
    };

    enum class alternate_function_t : std::uint8_t
    {
        AF0 = 0,
        AF1,
        AF2,
        AF3,
        AF4,
        AF5,
        AF6,
        AF7,
        AF8,
        AF9,
        AF10,
        AF11,
        AF12,
        AF13,
        AF14,
        AF15
    };

    struct config_t
    {
        mode_t               port_mode { mode_t::input };
        output_type_t        output_type { output_type_t::push_pull };
        output_speed_t       output_speed { output_speed_t::low };
        pull_up_down_t       pull_up_down { pull_up_down_t::no_pull_up_down };
        alternate_function_t alt_func { alternate_function_t::AF0 };
    };

    template<typename Registers, typename Fields, std::uintptr_t Address>
    struct pin_t
    {
        static constexpr auto address = Address;
        static constexpr auto number = Fields::number;

    private:
        static Registers* gpio() noexcept
        {
            return reinterpret_cast<Registers*>(Address);
        }

    public:
        [[nodiscard]] bool read_output() const noexcept
        {
            return gpio()->ODR.read(Fields::output);
        }
        [[nodiscard]] bool read() const noexcept
        {
            return gpio()->IDR.read(Fields::input);
        }
        void write(bool value) const noexcept
        {
            value ? set() : reset();
        }
        void set() const noexcept
        {
            gpio()->BSRR.set(Fields::set);
        }
        void reset() const noexcept
        {
            gpio()->BSRR.set(Fields::reset);
        }
        void configure(const config_t& cfg) const noexcept
        {
            auto* registers = gpio();
            registers->OTYPER.write(Fields::output_type, std::to_underlying(cfg.output_type));
            registers->OSPEEDR.write(Fields::output_speed, std::to_underlying(cfg.output_speed));
            registers->PUPDR.write(Fields::pull, std::to_underlying(cfg.pull_up_down));
            Fields::alternate_function_register(*registers)
                .write(Fields::alternate_function, std::to_underlying(cfg.alt_func));
            registers->MODER.write(Fields::mode, std::to_underlying(cfg.port_mode));
        }
    };
}
