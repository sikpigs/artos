#pragma once

#include <cstddef>
#include <mmio.h>
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

    template<typename Registers, std::uint32_t Pin, std::uintptr_t Address>
    requires(Pin < 16)
    struct pin_t
    {
        static_assert(offsetof(Registers, MODER) == 0x00);
        static_assert(offsetof(Registers, BSRR) == 0x18);
        static_assert(offsetof(Registers, AFRH) == 0x24);
        static constexpr auto address = Address;
        static constexpr auto number = Pin;

    private:
        static Registers* gpio() noexcept
        {
            return reinterpret_cast<Registers*>(Address);
        }
        template<typename Register, std::uint32_t Position, std::uint32_t Width = 1>
        static constexpr artos::mmio::field_t<typename Register::tag_type, Position, Width> field {};

    public:
        [[nodiscard]] bool read_output() const noexcept
        {
            return gpio()->ODR.read(field<decltype(Registers::ODR), Pin>);
        }
        [[nodiscard]] bool read() const noexcept
        {
            return gpio()->IDR.read(field<decltype(Registers::IDR), Pin>);
        }
        void write(bool value) const noexcept
        {
            value ? set() : reset();
        }
        void set() const noexcept
        {
            gpio()->BSRR.set(field<decltype(Registers::BSRR), Pin>);
        }
        void reset() const noexcept
        {
            gpio()->BSRR.set(field<decltype(Registers::BSRR), Pin + 16>);
        }
        void configure(const config_t& cfg) const noexcept
        {
            gpio()->OTYPER.write(field<decltype(Registers::OTYPER), Pin>, std::to_underlying(cfg.output_type));
            gpio()->OSPEEDR.write(field<decltype(Registers::OSPEEDR), Pin * 2, 2>,
                                  std::to_underlying(cfg.output_speed));
            gpio()->PUPDR.write(field<decltype(Registers::PUPDR), Pin * 2, 2>, std::to_underlying(cfg.pull_up_down));
            if constexpr (Pin < 8)
                gpio()->AFRL.write(field<decltype(Registers::AFRL), Pin * 4, 4>, std::to_underlying(cfg.alt_func));
            else
                gpio()->AFRH.write(field<decltype(Registers::AFRH), (Pin - 8) * 4, 4>,
                                   std::to_underlying(cfg.alt_func));
            gpio()->MODER.write(field<decltype(Registers::MODER), Pin * 2, 2>, std::to_underlying(cfg.port_mode));
        }
    };
}
