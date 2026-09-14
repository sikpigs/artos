#pragma once

#include <artos.h>
#include <bit>
#include <intrusive_list.h>
#include <limits>
#include <wait.h>

namespace artos::thread::internal {
    enum class thread_state_t : std::uint8_t
    {
        inactive,
        ready,
        running,
        blocked,
        suspended,
        sleeping,
        finished
    };

    struct tcb_t
    {
        tcb_t() noexcept
        {
            waiter.thread = this;
        }

        tcb_t(const tcb_t&) = delete;
        tcb_t& operator=(const tcb_t&) = delete;

        ~tcb_t() = default;

        stack_word_t* sp;
        stack_word_t* stack_begin;
        stack_word_t* stack_end;

        std::string_view name;
        priority_t       priority;
        thread_state_t   state = thread_state_t::inactive;
        tick_t           wake_tick = 0;

        list_hook_t ready;
        wait_node_t waiter;

        static constexpr std::uint32_t index_bits { std::numeric_limits<std::uint32_t>::digits
                                                    - static_cast<std::uint32_t>(
                                                        std::countl_zero(build_config::artos::max_threads)) };
        static constexpr std::uint32_t index_mask = ((1U << index_bits) - 1U);
        static constexpr std::uint32_t generation_bits { std::numeric_limits<std::uint32_t>::digits - index_bits };
        static constexpr std::uint32_t generation_mask = ((1U << generation_bits) - 1U) << index_bits;

        id_t id { generation_mask };

        constexpr id_t make_thread_id(const std::int32_t ix) noexcept
        {
            // XXX For now we'll just rollover the generation
            auto generation = id & generation_mask;
            generation >>= index_bits;
            ++generation;
            generation <<= index_bits;
            generation &= generation_mask;
            id = generation | (static_cast<std::uint32_t>(ix) & index_mask);
            return id;
        }
    };

    static_assert(sizeof(tcb_t) == tcb_size);
    static_assert(alignof(tcb_t) <= tcb_alignment);

}
