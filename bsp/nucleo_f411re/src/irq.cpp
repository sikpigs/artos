#include <irq.h>

namespace irq {
    namespace {
        struct slot_t
        {
            handler_t handler;
            void*     context;
        };

        slot_t usart2_slot {};

        class interrupt_guard_t
        {
            std::uint32_t saved_primask;

        public:
            interrupt_guard_t() noexcept
            {
                __asm volatile(
                    "mrs %0, primask\n"
                    "cpsid i"
                    : "=r"(saved_primask)::"memory");
            }

            ~interrupt_guard_t()
            {
                __asm volatile("msr primask, %0" ::"r"(saved_primask) : "memory");
            }

            interrupt_guard_t(const interrupt_guard_t&) = delete;
            interrupt_guard_t& operator=(const interrupt_guard_t&) = delete;
        };

        [[noreturn]] void unhandled_irq()
        {
            __asm volatile("bkpt #0");
            for (;;)
                __asm volatile("nop");
        }

        slot_t& slot_for(irq_t number)
        {
            switch (number)
            {
                case irq_t::usart2:
                    return usart2_slot;
            }
            unhandled_irq();
        }

        void dispatch(irq_t number)
        {
            slot_t callback;
            {
                const interrupt_guard_t guard;
                callback = slot_for(number);
            }

            if (callback.handler == nullptr)
                unhandled_irq();
            callback.handler(callback.context);
        }
    }

    void install(irq_t number, handler_t handler, void* context)
    {
        if (handler == nullptr)
            unhandled_irq();
        const interrupt_guard_t guard;
        slot_for(number) = { handler, context };
    }

    void uninstall(irq_t number)
    {
        // Prevent future dispatches; an already active callback can still finish.
        // The caller handles peripheral flags and stale NVIC pending state
        // before enabling this IRQ again.
        const interrupt_guard_t guard;
        disable(number);
        slot_for(number) = {};
    }

    void enable(irq_t number)
    {
        const interrupt_guard_t guard;
        if (slot_for(number).handler == nullptr)
            unhandled_irq();

        const auto  index = static_cast<std::uint32_t>(number);
        auto* const iser = reinterpret_cast<volatile std::uint32_t*>(0xE000E100UL);
        // Write-one-to-set: do not use a read/modify/write operation.
        iser[index / 32] = std::uint32_t { 1 } << (index % 32);
    }

    void disable(irq_t number)
    {
        (void)slot_for(number);
        const auto  index = static_cast<std::uint32_t>(number);
        auto* const icer = reinterpret_cast<volatile std::uint32_t*>(0xE000E180UL);
        // Write-one-to-clear; this does not acknowledge the peripheral source.
        icer[index / 32] = std::uint32_t { 1 } << (index % 32);
        __asm volatile("dsb\nisb" ::: "memory");
    }
}

extern "C" void USART2_IRQHandler()
{
    irq::dispatch(irq::irq_t::usart2);
}
