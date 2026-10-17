#pragma once

#include <variant>

#include <artos.h>
#include <expected_compat.h>
#include <wait_queue.h>

namespace artos {
    class event_t;

    using bits_t = port::stack_word_t;

    struct any_t
    {
        bits_t bits;
    };

    struct all_t
    {
        bits_t bits;
    };

    enum class event_error_t : std::uint8_t
    {
        timeout,
        closed,
        unsupported
    };

    using event_result_t = artos::expected<bits_t, event_error_t>;
    using any_or_all_t = std::variant<any_t, all_t>;

    namespace kernel::locked {
        bool event_satistifed(event_t* event, any_or_all_t bits) noexcept;
    }

    class event_t
    {
        enum class state_t : std::uint8_t
        {
            open,
            closed
        };

    public:
        event_t() noexcept
            : bitset {}
        {
        }

        explicit event_t(bits_t val) noexcept
            : bitset { val }
        {
        }

        event_t(const event_t&) = delete;
        event_t& operator=(const event_t&) = delete;

        event_result_t post(bits_t bits) noexcept;
        event_result_t set(bits_t bits) noexcept;
        event_result_t post_from_isr(bits_t bits, bool* higher_priority_thread_woken = nullptr) noexcept;
        event_result_t set_from_isr(bits_t bits, bool* higher_priority_thread_woken = nullptr) noexcept;

        [[nodiscard]] event_result_t wait_for(any_t bits, bool reset, tick_t ticks_to_wait) noexcept;
        [[nodiscard]] event_result_t wait_for(any_t bits, bool reset, wait_forever_t) noexcept;
        [[nodiscard]] event_result_t wait_for(all_t bits, bool reset, tick_t ticks_to_wait) noexcept;
        [[nodiscard]] event_result_t wait_for(all_t bits, bool reset, wait_forever_t) noexcept;

        void close() noexcept;
        void close_and_wait() noexcept;
        void close_and_delete() noexcept;

    private:
        class operation_guard_t
        {
        public:
            explicit operation_guard_t(event_t& owner) noexcept;
            ~operation_guard_t() noexcept;

            operation_guard_t(const operation_guard_t&) = delete;
            operation_guard_t& operator=(const operation_guard_t&) = delete;

            [[nodiscard]] bool entered() const noexcept
            {
                return active;
            }

        private:
            event_t& event;
            bool     active { false };
        };

        friend bool kernel::locked::event_satistifed(event_t* event, any_or_all_t bits) noexcept;

        void finish_operation() noexcept;
        bool satisfied(any_or_all_t bits) noexcept;
        void clear(any_or_all_t bits) noexcept;

        event_result_t wait(any_or_all_t bits, bool clear, std::optional<tick_t> ticks_to_wait) noexcept;

        bits_t bitset;

        state_t     state { state_t::open };
        bool        delete_on_close { false };
        std::size_t active_operations { 0 };

        wait_queue_t waiters {};
        wait_queue_t close_queue {};
    };
}