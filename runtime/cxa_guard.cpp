#include <build_config.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>

#include <array>
#include <cstdint>

#include <artos.h>
#include <bits.h>
#include <cpu_port.h>
#include <wait_queue.h>

namespace {

    constexpr auto initialized = bit<std::uint32_t, 0>;
    constexpr auto in_progress = bit<std::uint32_t, 1>;

    struct guard_runtime_t
    {
        volatile std::uint32_t* guard {};
        artos::thread::id_t     owner {};
        artos::wait_queue_t     waiters {};
    };

    std::array<guard_runtime_t, build_config::artos::max_active_guards> guard_runtimes {};

    [[nodiscard]] guard_runtime_t& get_guard_runtime(volatile std::uint32_t* guard) noexcept
    {
        guard_runtime_t* available = nullptr;

        for (auto& runtime : guard_runtimes)
        {
            if (runtime.guard == guard)
            {
                return runtime;
            }

            if (available == nullptr && runtime.guard == nullptr)
            {
                available = std::addressof(runtime);
            }
        }

        if (!available)
        {
            abort();
        }

        available->guard = guard;

        return *available;
    }

    extern "C" int __cxa_guard_acquire(volatile std::uint32_t* guard)
    {
        while (true)
        {
            const auto irq_state = cpu::port::irq_save_disable();
            const auto state = *guard;

            if (state & initialized)
            {
                cpu::port::irq_restore(irq_state);
                return 0;
            }

            auto& runtime = get_guard_runtime(guard);

            if (!(state & in_progress))
            {
                *guard = state | in_progress;
                runtime.owner = artos::thread::get_current_thread();
                cpu::port::data_memory_barrier();
                cpu::port::irq_restore(irq_state);
                return 1;
            }

            if (runtime.owner == artos::thread::get_current_thread() || cpu::port::is_in_irq())
            {
                // recursive initializations
                cpu::port::irq_restore(irq_state);
                cpu::port::fatal_trap();
            }

            artos::kernel::locked::block_current(runtime.waiters, artos::wait_forever);
            cpu::port::irq_restore(irq_state);
        }
    }

    extern "C" void __cxa_guard_release(volatile std::uint32_t* guard)
    {
        const auto irq_state = cpu::port::irq_save_disable();
        auto&      runtime = get_guard_runtime(guard);

        cpu::port::data_memory_barrier();
        *guard = initialized;
        runtime.owner = artos::thread::invalid_id;
        runtime.guard = nullptr;
        cpu::port::data_memory_barrier();

        if (artos::kernel::locked::wake_all(runtime.waiters))
        {
            artos::kernel::internal::sched();
        }

        cpu::port::irq_restore(irq_state);
    }

    extern "C" void __cxa_guard_abort(volatile std::uint32_t* guard)
    {
        const auto irq_state = cpu::port::irq_save_disable();
        auto&      runtime = get_guard_runtime(guard);

        *guard = 0;
        runtime.owner = artos::thread::invalid_id;
        runtime.guard = nullptr;
        cpu::port::data_memory_barrier();

        if (artos::kernel::locked::wake_all(runtime.waiters))
        {
            artos::kernel::internal::sched();
        }

        cpu::port::irq_restore(irq_state);
    }
}
