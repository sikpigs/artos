#pragma once

#include <build_config.h>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include <artos_port.h>
#include <cpu_port.h>
#include <defs.h>
#include <expected_compat.h>
#include <wait_queue.h>

namespace artos {
    using tick_t = std::uint32_t; // TODO this should be based on CPU, like word_t or something, then tick generation
                                  // could be conditional i.e. 64-bit no generation
    using err_t = int;

    struct wait_forever_t
    {
        explicit constexpr wait_forever_t() noexcept = default;
    };

    inline constexpr wait_forever_t wait_forever {};

    struct tick_time_t
    {
        tick_t ticks;
        tick_t generation;

        tick_time_t operator+(tick_t addend) const noexcept
        {
            const auto now = ticks;
            auto       gen = generation;

            auto tick = now + addend;
            if (tick < now)
            {
                ++gen;
            }

            return { tick, gen };
        }

        bool operator==(const tick_time_t&) const = default;

        constexpr tick_time_t operator-(const tick_time_t& subtrahend) const noexcept
        {
            const auto tick = tick_t { ticks - subtrahend.ticks };
            const auto borrow = static_cast<tick_t>(ticks < subtrahend.ticks);

            return { .ticks = tick, .generation = tick_t { generation - subtrahend.generation - borrow } };
        }

        constexpr tick_time_t operator+(const tick_time_t& addend) const noexcept
        {
            const auto tick = tick_t { ticks + addend.ticks };
            const auto carry = static_cast<tick_t>(tick < ticks);

            return { .ticks = tick, .generation = tick_t { generation + addend.generation + carry } };
        }

        std::strong_ordering operator<=>(const tick_time_t& other) const noexcept
        {
            if (generation != other.generation)
            {
                return generation <=> other.generation;
            }

            return ticks <=> other.ticks;
        }
    };

    inline constexpr auto immediately = tick_time_t { .ticks = 0, .generation = 0 };

    struct [[nodiscard]] global_interrupt_guard_t
    {
        global_interrupt_guard_t() noexcept
            : state { artos::port::irq_save_disable() }
        {
        }

        global_interrupt_guard_t(const global_interrupt_guard_t&) = delete;
        global_interrupt_guard_t& operator=(const global_interrupt_guard_t&) = delete;

        ~global_interrupt_guard_t() noexcept
        {
            artos::port::irq_restore(state);
        }

    private:
        artos::port::primask_t state;
    };

    struct [[nodiscard]] critical_section_t
    {
        critical_section_t() noexcept
            : previous_basepri { cpu::port::irq_save_raise_mask(port::kernel_interrupt_priority) }
        {
        }

        ~critical_section_t() noexcept
        {
            reset();
        }

        critical_section_t(const critical_section_t&) = delete;
        critical_section_t& operator=(const critical_section_t&) = delete;

        critical_section_t(critical_section_t&& other)
            : previous_basepri { std::exchange(other.previous_basepri,
                                               std::numeric_limits<cpu::port::basepri_t>::max()) }
        {
        }

        void reset() noexcept
        {
            if (previous_basepri != std::numeric_limits<cpu::port::basepri_t>::max())
            {
                cpu::port::irq_restore_mask(previous_basepri);
                previous_basepri = std::numeric_limits<cpu::port::basepri_t>::max();
            }
        }

    private:
        cpu::port::basepri_t previous_basepri;
    };

    namespace kernel {

        namespace internal {
            // Called by the selected timer ISR.
            void on_tick() noexcept;
        }

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

        void                      init() noexcept;
        [[noreturn]] void         start() noexcept;
        [[nodiscard]] tick_t      ticks() noexcept;
        [[nodiscard]] tick_time_t tick_time() noexcept;

        namespace locked {
            [[nodiscard]] wake_reason_t block_current(wait_queue_t&      queue,
                                                      const tick_time_t& deadline,
                                                      critical_section_t cs) noexcept;
            [[nodiscard]] wake_reason_t block_current(wait_queue_t& queue,
                                                      wait_forever_t,
                                                      critical_section_t cs) noexcept;

            void block_current(wait_queue_t& queue, const tick_time_t& deadline) noexcept;
            void block_current(wait_queue_t& queue, wait_forever_t) noexcept;

            [[nodiscard]] tick_time_t tick_time() noexcept;

            bool wake_all(wait_queue_t& queue) noexcept;
            bool wake_one(wait_queue_t& queue) noexcept;
        }
    }

    namespace thread {
        using id_t = std::uintptr_t;
        using func_t = void (*)(void*);
        using start_result_t = artos::expected<id_t, err_t>;
        using stack_word_t = port::stack_word_t;

        inline constexpr id_t invalid_id = std::numeric_limits<id_t>::max();

        // Total allocated bytes, including saved context. This is a baseline
        // for kernel calls in Debug, not a guarantee for arbitrary task code.
        inline constexpr std::uint32_t minimum_stack_bytes = 1024;
        static_assert(minimum_stack_bytes >= port::initial_frame_bytes);
        static_assert(minimum_stack_bytes % port::stack_alignment == 0);

        inline constexpr auto tcb_size
            = static_cast<std::size_t>(88) + (build_config::artos::scheduler.priority_aging_ticks.has_value() ? 8 : 0);
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
                                  stack_word_t* const stack,
                                  bool                start) noexcept;
        }

        template<std::uint32_t StackSize>
        requires(StackSize >= minimum_stack_bytes && StackSize % port::stack_alignment == 0)
        [[nodiscard]]
        start_result_t create(func_t                     func,
                              std::string_view           name,
                              void*                      parameters,
                              priority_t                 priority,
                              thread_storage<StackSize>& storage,
                              bool                       start = true) noexcept
        {
            return internal::create(
                func, name, StackSize, parameters, priority, storage.tcb.data(), storage.stack.data(), start);
        }

        id_t get_current_thread() noexcept;
        void sleep_for(tick_t ticks) noexcept;
        void sleep_until(const tick_time_t& ticks) noexcept;
    }
}
