#pragma once

#include <artos.h>
#include <internal/thread_id.h>
#include <intrusive_list.h>
#include <wait.h>

namespace artos::thread::internal {
    enum class thread_state_t : std::uint8_t
    {
        inactive,
        ready,
        running,
        blocked,
        blocked_forever,
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
        thread_state_t   state { thread_state_t::inactive };

        tick_time_t deadline {};

        wait_node_t waiter;

        using id_layout = thread_id_layout_t<id_t, build_config::artos::max_threads>;

        id_t id { id_layout::initial_id };

        constexpr id_t make_thread_id(std::size_t index) noexcept
        {
            id = id_layout::next(id, index);
            return id;
        }
    };

    static_assert(sizeof(tcb_t) == tcb_size);
    static_assert(alignof(tcb_t) <= tcb_alignment);

}
