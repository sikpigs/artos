#pragma once

#include <cstdint>

namespace irq {
    enum class irq_t : std::uint32_t
    {
        usart2 = 38,
    };

    using handler_t = void (*)(void* context);

    void install(irq_t number, handler_t, void* context = nullptr);
    void uninstall(irq_t number);

    void enable(irq_t number);
    void disable(irq_t number);

    template<auto Handler, typename Peripheral, typename Context>
    void install(const Peripheral&, Context& ctx)
    {
        install(
            Peripheral::irq_number,
            +[](void* erased) {
                auto& obj = *static_cast<Context*>(erased);
                (obj.*Handler)();
            },
            &ctx);
    }
}