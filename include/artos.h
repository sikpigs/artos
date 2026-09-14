#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected_compat.h>
#include <string_view>
#include <utility>

#include <artos_port.h>
#include <build_config.h>
#include <wait.h>

namespace artos {
    using tick_t = std::uint32_t;
    using err_t = int;

    struct [[nodiscard]] critical_section_t
    {
        critical_section_t() noexcept
            : state { artos::port::irq_save_disable() }
        {
        }

        critical_section_t(const critical_section_t&) = delete;
        critical_section_t& operator=(const critical_section_t&) = delete;

        ~critical_section_t() noexcept
        {
            artos::port::irq_restore(state);
        }

    private:
        artos::port::primask_t state;
    };

    namespace kernel {
        namespace internal {
            // Called by the selected timer ISR.
            void on_tick() noexcept;
        }

        inline constexpr tick_t max_sleep_ticks = (tick_t { 1 } << 31) - 1;

        [[nodiscard]]
        constexpr tick_t ms_to_ticks(std::uint64_t ms) noexcept
        {
            return static_cast<tick_t>(ms * build_config::artos::tick_hz / 1000u);
        }

        [[nodiscard]]
        constexpr std::uint64_t ticks_to_ms(tick_t ticks) noexcept
        {
            return static_cast<std::uint64_t>(ticks) * 1000u / build_config::artos::tick_hz;
        }

        void                 init() noexcept;
        [[noreturn]] void    start() noexcept;
        [[nodiscard]] tick_t ticks() noexcept;

        namespace locked {
            void block_current(wait_queue_t& queue) noexcept;
            bool wake_one(wait_queue_t& queue) noexcept;
        }
    }

    namespace thread {
        using id_t = std::uintptr_t;
        using func_t = void (*)(void*);
        using start_result_t = artos::expected<id_t, err_t>;
        using stack_word_t = port::stack_word_t;

        // Total allocated bytes, including saved context. This is a baseline
        // for kernel calls in Debug, not a guarantee for arbitrary task code.
        inline constexpr std::uint32_t minimum_stack_bytes = 1024;
        static_assert(minimum_stack_bytes >= port::initial_frame_bytes);
        static_assert(minimum_stack_bytes % port::stack_alignment == 0);

        inline constexpr auto tcb_size = static_cast<std::size_t>(60);
        inline constexpr auto tcb_alignment = alignof(std::max_align_t);

        enum class priority_t : std::uint8_t
        {
            high = 0,
            low = build_config::artos::priority_levels - 1
        };

        template<std::uint8_t Priority>
        requires(Priority <= std::to_underlying(priority_t::low))
        constexpr priority_t make_priority() noexcept
        {
            return static_cast<priority_t>(Priority);
        }

        template<std::uint32_t StackSize>
        requires(StackSize >= minimum_stack_bytes && StackSize % port::stack_alignment == 0)
        struct thread_storage
        {
            alignas(tcb_alignment) std::array<std::byte, tcb_size> tcb {};
            alignas(port::stack_alignment) std::array<stack_word_t, StackSize / sizeof(stack_word_t)> stack {};
        };

        namespace internal {
            extern "C" stack_word_t* artos_context_switch(stack_word_t* outgoing_sp) noexcept;

            start_result_t create(func_t              func,
                                  std::string_view    name,
                                  std::uint32_t       stack_size_bytes,
                                  void*               parameters,
                                  priority_t          priority,
                                  std::byte* const    tcb,
                                  stack_word_t* const stack) noexcept;
        }

        template<std::uint32_t StackSize>
        requires(StackSize >= minimum_stack_bytes && StackSize % port::stack_alignment == 0)
        [[nodiscard]]
        start_result_t create(func_t                     func,
                              std::string_view           name,
                              void*                      parameters,
                              priority_t                 priority,
                              thread_storage<StackSize>& storage) noexcept
        {
            return internal::create(
                func, name, StackSize, parameters, priority, storage.tcb.data(), storage.stack.data());
        }

        void sleep_for(tick_t ticks) noexcept;
        void sleep_until(tick_t ticks) noexcept;
    }
}
