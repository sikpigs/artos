#include <artos.h>
#include <artos_port.h>
#include <bits.h>
#include <board.h>
#include <build_config.h>
#include <mmio.h>

namespace artos::port {
    namespace {
        constexpr auto clock_hz = board::chosen::kernel_tick::clock::frequency_hz;
        constexpr auto tick_hz = build_config::artos::tick_hz;
        static_assert(clock_hz >= tick_hz);
        static_assert(clock_hz % tick_hz == 0, "tick frequency must divide the timer clock");
        constexpr auto reload = clock_hz / tick_hz - 1;
        static_assert(reload > 0 && reload <= 0x00FFFFFF, "SysTick reload must fit 24 bits");
        namespace regmap {
            namespace tags {
                struct ctrl;
                struct load;
                struct val;
            }

            using CTRL = register_t<tags::ctrl>;
            using LOAD = register_t<tags::load>;
            using VAL = register_t<tags::val>;

            namespace ctrl {
                inline constexpr field_t<tags::ctrl, 0>  enable {};
                inline constexpr field_t<tags::ctrl, 1>  tickint {};
                inline constexpr field_t<tags::ctrl, 2>  clocksource {};
                inline constexpr field_t<tags::ctrl, 16> countflag {};
            }

            namespace load {
                inline constexpr field_t<tags::load, 0, 24> load {};
            }

            namespace val {
                inline constexpr field_t<tags::val, 0, 24> val {};
            }

            struct registers_t
            {
                regmap::CTRL CTRL;
                regmap::LOAD LOAD;
                regmap::VAL  VAL;
            };

            static_assert(alignof(registers_t) == alignof(std::uint32_t));
            static_assert(offsetof(registers_t, CTRL) == 0x00);
            static_assert(offsetof(registers_t, LOAD) == 0x04);
            static_assert(offsetof(registers_t, VAL) == 0x08);

            static constexpr auto     systick_address = board::chosen::kernel_tick::address;
            inline registers_t* const SYSTICK = reinterpret_cast<registers_t*>(systick_address);

        }

    }

    void start_tick() noexcept
    {
        regmap::SYSTICK->CTRL.write(0);
        regmap::SYSTICK->LOAD.write(reload);
        regmap::SYSTICK->VAL.write(0);
        regmap::SYSTICK->CTRL.set(regmap::ctrl::enable, regmap::ctrl::tickint, regmap::ctrl::clocksource);
    }

    stack_word_t* initialize_stack(stack_word_t* stack_top, void (*entry)(void*), void* argument) noexcept
    {
        auto* sp = stack_top;
        *(--sp) = bit<stack_word_t, 24>;
        *(--sp) = reinterpret_cast<stack_word_t>(entry);
        *(--sp) = 0xFFFFFFFFU;                              // LR: trap if the thread returns.
        *(--sp) = 0;                                        // R12
        *(--sp) = 0;                                        // R3
        *(--sp) = 0;                                        // R2
        *(--sp) = 0;                                        // R1
        *(--sp) = reinterpret_cast<stack_word_t>(argument); // R0
        *(--sp) = 0;                                        // R11
        *(--sp) = 0;                                        // R10
        *(--sp) = 0;                                        // R9
        *(--sp) = 0;                                        // R8
        *(--sp) = 0;                                        // R7
        *(--sp) = 0;                                        // R6
        *(--sp) = 0;                                        // R5
        *(--sp) = 0;                                        // R4

        return sp;
    }
}

extern "C" void SysTick_Handler() noexcept
{
    artos::kernel::internal::on_tick();
}
