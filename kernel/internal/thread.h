#pragma once

#include <build_config.h>

#include <internal/thread_id.h>
#include <internal/thread_queue.h>

#include <optional>

#include <artos.h>
#include <intrusive_list.h>
#include <wait_queue.h>

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

    struct tcb_base_t
    {
        tcb_base_t() noexcept
        {
            ready.thread = this;
            waiter.thread = this;
            block_sleep.thread = this;
        }

        tcb_base_t(const tcb_base_t&) = delete;
        tcb_base_t& operator=(const tcb_base_t&) = delete;

        ~tcb_base_t() = default;

        stack_word_t* sp;
        stack_word_t* stack_begin;
        stack_word_t* stack_end;

        std::string_view name;
        priority_t       priority;
        thread_state_t   state { thread_state_t::inactive };

        tick_time_t deadline {};

        thread_queue_node_t ready;
        thread_queue_node_t block_sleep;
        wait_queue_node_t   waiter;

        using id_layout = thread_id_layout_t<id_t, build_config::artos::max_threads>;

        id_t id { id_layout::initial_id };

        constexpr id_t make_thread_id(std::size_t index) noexcept
        {
            id = id_layout::next(id, index);
            return id;
        }
    };

    template<typename Scheduler>
    struct tcb_t;

    template<>
    struct tcb_t<const build_config::artos::fixed_priority_round_robin_config> : tcb_base_t
    {
    };

    void                      set_priority_ready(priority_t priority) noexcept;
    void                      clear_priority_ready(priority_t priority) noexcept;
    std::optional<priority_t> get_highest_set_priority() noexcept;

    void make_thread_ready(tcb_base_t* thread) noexcept;
    void block_sleep_thread(tcb_base_t* thread) noexcept;

#ifndef ARTOS_HOST_TEST
    static_assert(sizeof(tcb_t<decltype(build_config::artos::scheduler)>) == tcb_size);
    static_assert(alignof(tcb_t<decltype(build_config::artos::scheduler)>) <= tcb_alignment);
#endif
}
