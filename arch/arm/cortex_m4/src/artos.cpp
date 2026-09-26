#include <artos.h>
#include <artos_port.h>
#include <bits.h>
#include <board.h>
#include <build_config.h>
#include <mmio.h>

#include <arch/regmap.h>

namespace artos::port {
    namespace {
        constexpr auto clock_hz = board::chosen::kernel_tick::clock::frequency_hz;
        constexpr auto tick_hz = build_config::artos::tick_hz;
        static_assert(clock_hz >= tick_hz);
        static_assert(clock_hz % tick_hz == 0, "tick frequency must divide the timer clock");
        constexpr auto reload = clock_hz / tick_hz - 1;
        static_assert(reload > 0 && reload <= 0x00FFFFFF, "SysTick reload must fit 24 bits");
        static_assert(build_config::artos::scheduler_min_cycles_remaining < reload,
                      "scheduler cutoff must be shorter than a SysTick period");
        namespace systick_regmap {
            namespace tags {
                struct ctrl;
                struct load;
                struct val;
            }

            using CTRL = artos::mmio::register_t<tags::ctrl>;
            using LOAD = artos::mmio::register_t<tags::load>;
            using VAL = artos::mmio::register_t<tags::val>;

            namespace ctrl {
                inline constexpr artos::mmio::field_t<tags::ctrl, 0>  enable {};
                inline constexpr artos::mmio::field_t<tags::ctrl, 1>  tickint {};
                inline constexpr artos::mmio::field_t<tags::ctrl, 2>  clocksource {};
                inline constexpr artos::mmio::field_t<tags::ctrl, 16> countflag {}; // NOLINT
            }

            namespace load {
                inline constexpr artos::mmio::field_t<tags::load, 0, 24> load {}; // NOLINT
            }

            namespace val {
                inline constexpr artos::mmio::field_t<tags::val, 0, 24> val {}; // NOLINT
            }

            // Preserve the hardware register names used by the Cortex-M documentation.
            // NOLINTBEGIN(readability-identifier-naming)
            struct registers_t
            {
                systick_regmap::CTRL CTRL;
                systick_regmap::LOAD LOAD;
                systick_regmap::VAL  VAL;
            };
            // NOLINTEND(readability-identifier-naming)

            static_assert(alignof(registers_t) == alignof(std::uint32_t));
            static_assert(offsetof(registers_t, CTRL) == 0x00);
            static_assert(offsetof(registers_t, LOAD) == 0x04);
            static_assert(offsetof(registers_t, VAL) == 0x08);

            static constexpr auto     systick_address = board::chosen::kernel_tick::address;
            inline registers_t* const SYSTICK = reinterpret_cast<registers_t*>(systick_address);
        }
    }

    void init() noexcept
    {
        using namespace artos::port::regmap;
        // Lowest priority on STM32F411 (four implemented priority bits).
        SCB->SHPR3.write(scb::shpr3::pendsv(0xF0), scb::shpr3::systick(0xF0));
        SCB->SHPR2.write(scb::shpr2::svc(0));
        FPU->FPCCR.set(fpu::fpccr::aspen, fpu::fpccr::lspen);
    }

    void start_tick() noexcept
    {
        systick_regmap::SYSTICK->CTRL.write(0);
        systick_regmap::SYSTICK->LOAD.write(reload);
        systick_regmap::SYSTICK->VAL.write(0);
        systick_regmap::SYSTICK->CTRL.set(
            systick_regmap::ctrl::enable, systick_regmap::ctrl::tickint, systick_regmap::ctrl::clocksource);
    }

    std::uint32_t get_cycles_remaining() noexcept
    {
        return systick_regmap::SYSTICK->VAL.read();
    }

    void pendsvset() noexcept
    {
        using namespace artos::port::regmap;
        SCB->ICSR.set(scb::icsr::pendsvset);
    }

    extern "C" void thread_exit() noexcept;

    stack_word_t* initialize_stack(stack_word_t* stack_top, void (*entry)(void*), void* argument) noexcept
    {
        auto* sp = stack_top;
        *(--sp) = bit<stack_word_t, 24>;
        *(--sp) = reinterpret_cast<stack_word_t>(entry);
        *(--sp) = reinterpret_cast<stack_word_t>(&thread_exit); // LR
        *(--sp) = 0;                                            // R12
        *(--sp) = 0;                                            // R3
        *(--sp) = 0;                                            // R2
        *(--sp) = 0;                                            // R1
        *(--sp) = reinterpret_cast<stack_word_t>(argument);     // R0
        *(--sp) = artos::port::initial_exc_return;              // EXC_RETURN
        *(--sp) = 0;                                            // R11
        *(--sp) = 0;                                            // R10
        *(--sp) = 0;                                            // R9
        *(--sp) = 0;                                            // R8
        *(--sp) = 0;                                            // R7
        *(--sp) = 0;                                            // R6
        *(--sp) = 0;                                            // R5
        *(--sp) = 0;                                            // R4

        return sp;
    }
}

extern "C" void SysTick_Handler() noexcept
{
    // SysTick and PendSV have the same preemption priority, so their scheduler
    // operations cannot overlap. Thread-side registry changes mask interrupts.
    // Keep higher-priority UART RX enabled during the scan: a global mask here
    // can overrun the receive register during a full-speed paste in Debug.
    // Higher-priority ISRs must not modify scheduler state under this policy.
    artos::kernel::internal::on_tick();
}
