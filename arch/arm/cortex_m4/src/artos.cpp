#include <arch/artos.h>
#include <build_config.h>

#include <arch/fault_capture.h>
#include <arch/regmap.h>

#include <artos.h>
#include <bits.h>
#include <board.h>
#include <port/artos_port.h>
#include <port/cpu_port.h>

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
        auto* const systick_registers = regmap::systick::at(board::chosen::kernel_tick::address);
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
        systick_registers->CTRL.write(0);
        systick_registers->LOAD.write(reload);
        systick_registers->VAL.write(0);
        systick_registers->CTRL.set(
            regmap::systick::ctrl::enable, regmap::systick::ctrl::tickint, regmap::systick::ctrl::clocksource);
    }

    std::uint32_t get_cycles_remaining() noexcept
    {
        return systick_registers->VAL.read();
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

    validate_stack_protection_result_t validate_state_protection(stack_bounds_t stack_bounds) noexcept
    {
        if (stack_bounds.bottom >= stack_bounds.top)
        {
            return validate_stack_protection_result_t::invalid;
        }

        if ((reinterpret_cast<std::uint32_t>(stack_bounds.top) % 8) != 0)
        {
            return validate_stack_protection_result_t::invalid;
        }

        if ((reinterpret_cast<std::uint32_t>(stack_bounds.bottom) % stack_layout_requirements().usable_bottom_alignment)
            != 0)
        {
            return validate_stack_protection_result_t::invalid;
        }

        return validate_stack_protection_result_t::success;
    }

    void activate_stack_protection(stack_bounds_t stack_bounds) noexcept
    {
        using namespace regmap;

        const auto guard_end = reinterpret_cast<std::uintptr_t>(stack_bounds.bottom);
        const auto guard_begin = guard_end - stack_layout_requirements().reserved_prefix_bytes;

        cpu::port::data_memory_barrier();

        MPU->CTRL.write(0);
        MPU->RNR.write(0);

        const auto rbar_value = mpu::rbar::address((reinterpret_cast<std::uintptr_t>(stack_bounds.bottom)
                                                    - stack_layout_requirements().reserved_prefix_bytes)
                                                   >> 5)
                                    .encoded_value
                                | mpu::rbar::valid(1).encoded_value;
        MPU->RBAR.write(rbar_value);

        constexpr auto rasr_value
            = mpu::rasr::enable(1).encoded_value | mpu::rasr::size(4).encoded_value | mpu::rasr::xn(1).encoded_value;
        MPU->RASR.write(rasr_value);

        constexpr auto ctrl_value = mpu::ctrl::enable(1).encoded_value | mpu::ctrl::privdefena(1).encoded_value;
        MPU->CTRL.write(ctrl_value);
        SCB->SHCSR.set(scb::shcsr::memfaultena);

        __asm volatile("dsb" ::: "memory");
        __asm volatile("isb" ::: "memory");

        ::fault::set_active_stack_guard(guard_begin, guard_end);
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
