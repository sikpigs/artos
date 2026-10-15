#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>

#include <build_config.h>

#include <arch/artos.h>
#include <port/cpu_port.h>

#include <defs.h>
#include <expected_compat.h>
#include <intrusive_list.h>
#include <port/artos_port.h>
#include <wait_queue.h>

namespace artos {
    using tick_t = port::tick_t;
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

        void              init() noexcept;
        [[noreturn]] void start() noexcept;

        [[nodiscard]] tick_t      ticks() noexcept;
        [[nodiscard]] tick_time_t tick_time() noexcept;

        void yield(bool higher_priority_thread_woken) noexcept;
        void yield() noexcept;

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

            bool wake_all(wait_queue_t& queue, wake_reason_t reason = wake_reason_t::signaled) noexcept;
            bool wake_one(wait_queue_t& queue) noexcept;
        }
    }

    namespace thread {
        using id_t = std::uintptr_t;
        using name_t = std::array<char, build_config::artos::max_thread_name_length + 1>;
        using func_t = void (*)(void*);
        using create_result_t = artos::expected<id_t, err_t>;
        using stack_word_t = port::stack_word_t;

        inline constexpr auto canary_word_count = build_config::artos::thread_stack_canary ? 2 : 0;
        static_assert(!build_config::artos::thread_stack_canary
                      || canary_word_count * sizeof(stack_word_t) % port::stack_alignment == 0);
        inline constexpr id_t invalid_id = std::numeric_limits<id_t>::max();

        // Total allocated bytes, including saved context. This is a baseline
        // for kernel calls in Debug, not a guarantee for arbitrary task code.
        inline constexpr std::uint32_t minimum_stack_bytes = 1024;
        static_assert(minimum_stack_bytes >= port::initial_frame_bytes);
        static_assert(minimum_stack_bytes % port::stack_alignment == 0);

        enum class state_t : std::uint8_t
        {
            inactive,
            ready,
            running,
            blocked,
            indefinite,
            sleeping,
            finished
        };

        enum class priority_t : std::uint8_t
        {
            high = 0,
            low = build_config::artos::priority_levels - 1
        };

        enum class kill_result_t
        {
            success,
            invalid_thread
        };

        enum class join_result_t : std::uint8_t
        {
            success,
            deadlock_detected,
            invalid_thread,
            timeout
        };

        enum class suspend_result_t : std::uint8_t
        {
            success,
            invalid_thread,
            unsupported
        };

        enum class resume_result_t : std::uint8_t
        {
            success,
            invalid_thread,
            unsupported,
        };

        struct statistics_t
        {
            std::size_t execution_count {};
            // Each bucket stores accumulated CPU cycles, independent of its time span.
            std::array<std::uint32_t, build_config::artos::runtime_history_bucket_count> runtime_history_cycles {};
        };

        struct snapshot_t
        {
            id_t         id;
            name_t       name;
            state_t      state;
            priority_t   priority;
            bool         suspended;
            statistics_t statistics;
        };

        using snapshot_array_t = std::array<snapshot_t, build_config::artos::max_threads>;

        namespace internal {
            // Describe storage without including kernel-private TCB or queue
            // headers. Kernel assertions verify this model against real types.
            struct storage_layout_t
            {
                std::size_t size {};
                std::size_t alignment { 1 };

                constexpr void append(std::size_t field_size,
                                      std::size_t field_alignment,
                                      std::size_t count = 1) noexcept
                {
                    size = (size + field_alignment - 1) / field_alignment * field_alignment;
                    size += field_size * count;
                    alignment = alignment < field_alignment ? field_alignment : alignment;
                }

                template<typename Field>
                constexpr void append(std::size_t count = 1) noexcept
                {
                    append(sizeof(Field), alignof(Field), count);
                }

                [[nodiscard]] constexpr std::size_t padded_size() const noexcept
                {
                    return (size + alignment - 1) / alignment * alignment;
                }
            };

            inline constexpr auto thread_queue_storage_layout = [] {
                storage_layout_t layout;
                layout.append<list_hook_t>();
                layout.append<void*>(2); // Thread and queue pointers.
                return layout;
            }();

            consteval std::size_t calculate_tcb_size() noexcept
            {
                storage_layout_t layout;
                layout.append<name_t>();
                layout.append<priority_t>(2); // Effective and base priorities.
                layout.append<state_t>();
                layout.append<bool>();
                layout.append<stack_word_t*>(3); // SP, stack begin and end.
                layout.append(thread_queue_storage_layout.padded_size(), thread_queue_storage_layout.alignment, 2);
                layout.append<wait_queue_node_t>();
                layout.append<wait_queue_t>();
                layout.append<list_hook_t>();
                layout.append<intrusive_list_t>(); // Owned mutexes.
                layout.append<void*>();            // Waiting mutex.
                layout.append<tick_time_t>();
                layout.append<id_t>();
                layout.append<func_t>();
                layout.append<void*>();             // Exit callback context.
                layout.size = layout.padded_size(); // Complete tcb_base_t.

                if constexpr (build_config::artos::scheduler.priority_aging_ticks.has_value())
                {
                    storage_layout_t aging; // NOLINT(misc-const-correctness)
                    aging.append<std::optional<priority_t>>();
                    aging.append<tick_t>();
                    layout.append(aging.padded_size(), aging.alignment);
                }
                if constexpr (build_config::artos::thread_stack_hwm)
                {
                    layout.append<std::size_t>();
                }
                if constexpr (build_config::artos::thread_stack_canary)
                {
                    layout.append<stack_word_t*>();
                }
                if constexpr (build_config::artos::thread_stats)
                {
                    storage_layout_t stats; // NOLINT(misc-const-correctness)
                    stats.append<std::uint32_t>();
                    stats.append<statistics_t>();
                    layout.append(stats.padded_size(), stats.alignment);
                }
                return layout.padded_size();
            }
        }

        inline constexpr auto tcb_size = internal::calculate_tcb_size();
        inline constexpr auto tcb_alignment = alignof(std::max_align_t);

        template<bool Enabled>
        struct canary_storage_t
        {
            stack_word_t* data() noexcept
            {
                return nullptr;
            }
        };

        template<>
        struct canary_storage_t<true>
        {
            alignas(port::stack_alignment) std::array<stack_word_t, canary_word_count> words {};

            stack_word_t* data() noexcept
            {
                return words.data();
            }
        };

        inline constexpr auto stack_layout = build_config::artos::thread_stack_protection
                                                 ? port::stack_layout_requirements()
                                                 : port::stack_layout_requirements_t {
                                                       .usable_bottom_alignment = port::stack_alignment,
                                                       .reserved_prefix_bytes = 0,
                                                       .usable_size_alignment = port::stack_alignment,
                                                   };

        template<std::uint32_t StackSize>
        requires(StackSize >= minimum_stack_bytes && StackSize % port::stack_alignment == 0
                 && StackSize % stack_layout.usable_size_alignment == 0)
        struct storage_t
        {
            static constexpr auto        stack_size = StackSize;
            static constexpr std::size_t guard_words = stack_layout.reserved_prefix_bytes / sizeof(stack_word_t);
            static constexpr std::size_t usable_words = stack_size / sizeof(stack_word_t);

            alignas(tcb_alignment) std::array<std::byte, tcb_size> tcb {};
            [[no_unique_address]] canary_storage_t<build_config::artos::thread_stack_canary> canary {};
            alignas(stack_layout.usable_bottom_alignment)
                std::array<stack_word_t, guard_words + usable_words> stack_allocation {};

            stack_word_t* usable_stack_begin() noexcept
            {
                return stack_allocation.data() + guard_words;
            }
        };

        namespace internal {
            void on_exit(id_t thread_id, func_t at_exit, void* ctx = nullptr) noexcept;

            create_result_t create(func_t              func,
                                   std::string_view    name,
                                   std::uint32_t       stack_size_bytes,
                                   void*               parameters,
                                   priority_t          priority,
                                   std::byte* const    tcb,
                                   stack_word_t* const canary,
                                   stack_word_t* const stack,
                                   bool                create_suspended = false) noexcept;
        }

        template<std::uint8_t Priority>
        requires(Priority <= std::to_underlying(priority_t::low))
        constexpr priority_t make_priority() noexcept
        {
            return static_cast<priority_t>(Priority);
        }

        struct create_options_t
        {
            bool create_suspended;
        };

        inline constexpr create_options_t create_suspended = { .create_suspended = true };
        inline constexpr create_options_t make_ready = { .create_suspended = false };

        template<std::uint32_t StackSize>
        requires(StackSize >= minimum_stack_bytes && StackSize % port::stack_alignment == 0)
        [[nodiscard]]
        create_result_t create(func_t           func,
                               std::string_view name,
                               void*            parameters,
                               priority_t       priority,
                               create_options_t options = make_ready) noexcept
        {
            using storage_t = thread::storage_t<StackSize>;
            auto storage = std::make_unique_for_overwrite<storage_t>();

            auto result = internal::create(func,
                                           name,
                                           StackSize,
                                           parameters,
                                           priority,
                                           storage->tcb.data(),
                                           storage->canary.data(),
                                           storage->usable_stack_begin(),
                                           options.create_suspended);

            if (result)
            {
                internal::on_exit(
                    *result, +[](void* ctx) { delete reinterpret_cast<storage_t*>(ctx); }, storage.release());
                return *result;
            }
            else
            {
                return artos::unexpected(result.error());
            }
        }

        template<std::uint32_t StackSize>
        requires(StackSize >= minimum_stack_bytes && StackSize % port::stack_alignment == 0)
        [[nodiscard]]
        create_result_t create(func_t                func,
                               std::string_view      name,
                               void*                 parameters,
                               priority_t            priority,
                               storage_t<StackSize>& storage,
                               create_options_t      options = make_ready) noexcept
        {
            return internal::create(func,
                                    name,
                                    StackSize,
                                    parameters,
                                    priority,
                                    storage.tcb.data(),
                                    storage.canary.data(),
                                    storage.usable_stack_begin(),
                                    options.create_suspended);
        }

        [[nodiscard]] id_t        get_current_thread() noexcept;
        [[nodiscard]] std::size_t get_high_watermark() noexcept;

        [[noreturn]] void exit() noexcept;
        kill_result_t     kill(id_t id) noexcept;
        join_result_t     join(id_t id, tick_t ticks_to_wait) noexcept;
        join_result_t     join(id_t id, wait_forever_t) noexcept;
        suspend_result_t  suspend(id_t id) noexcept;
        resume_result_t   resume(id_t id) noexcept;
        resume_result_t   resume_from_isr(id_t id, bool* higher_priority_thread_woken = nullptr) noexcept;

        void get_statistics(statistics_t& statistics) noexcept;

        void sleep_for(tick_t ticks) noexcept;
        void sleep_until(const tick_time_t& ticks) noexcept;

        std::size_t get_snapshot(snapshot_array_t& snapshot) noexcept;
    }
}
