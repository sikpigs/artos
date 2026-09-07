#pragma once

#include <artos_port.h>
#include <build_config.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>

namespace artos {
    using tick_t = std::uint32_t;
    using err_t = std::ptrdiff_t;

    namespace kernel {
        namespace internal {
            // Called by the selected timer ISR.
            void on_tick() noexcept;
        }

        class kernel_t
        {
        public:
            kernel_t() noexcept;

        private:
        };
    }

    namespace thread {
        using id_t = std::uintptr_t;
        using func_t = void (*)(void*);
        using start_result_t = std::expected<id_t, err_t>;
        using stack_word_t = port::stack_word_t;

        inline constexpr auto tcb_size = static_cast<std::size_t>(28);

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
        requires(StackSize >= port::initial_frame_bytes && StackSize % port::stack_alignment == 0)
        struct thread_storage
        {
            alignas(tcb_alignment) std::array<std::byte, tcb_size> tcb {};
            alignas(port::stack_alignment) std::array<stack_word_t, StackSize / sizeof(stack_word_t)> stack {};
        };

        namespace internal {
            start_result_t create(func_t              func,
                                  std::string_view    name,
                                  std::uint32_t       stack_size_bytes,
                                  void*               parameters,
                                  priority_t          priority,
                                  std::byte* const    tcb,
                                  stack_word_t* const stack);
        }

        template<std::uint32_t StackSize>
        requires(StackSize >= port::initial_frame_bytes && StackSize % port::stack_alignment == 0)
        start_result_t create(func_t                     func,
                              std::string_view           name,
                              void*                      parameters,
                              priority_t                 priority,
                              thread_storage<StackSize>& storage) noexcept
        {
            return internal::create(
                func, name, StackSize, parameters, priority, storage.tcb.data(), storage.stack.data());
        }

    }
}
