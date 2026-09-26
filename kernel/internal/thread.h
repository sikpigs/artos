#pragma once

#include <build_config.h>

#include <internal/thread_id.h>
#include <internal/thread_queue.h>

#include <optional>
#include <utility>

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

    struct tcb_base_t;

    void                      set_priority_ready(priority_t priority) noexcept;
    void                      clear_priority_ready(priority_t priority) noexcept;
    std::optional<priority_t> get_highest_set_priority() noexcept;

    void make_thread_ready(tcb_base_t* thread) noexcept;
    void block_sleep_thread(tcb_base_t* thread) noexcept;
    void set_effective_priority(tcb_base_t* thread, priority_t new_priority) noexcept;

    struct tcb_base_t
    {
        tcb_base_t(std::string_view thread_name,
                   priority_t       thread_priority,
                   stack_word_t*    stack_pointer,
                   stack_word_t*    stack_start,
                   auto             stack_word_count) noexcept
            : name { thread_name }
            , priority { thread_priority }
            , state { thread_state_t::inactive }
            , sp { stack_pointer }
            , stack_begin { stack_start }
            , stack_end { stack_start + stack_word_count }
            , ready { this }
            , block_sleep { this }
            , waiter { this }
        {
        }

        tcb_base_t(const tcb_base_t&) = delete;
        tcb_base_t& operator=(const tcb_base_t&) = delete;

        ~tcb_base_t() noexcept = default;

        std::string_view name;
        priority_t       priority;
        thread_state_t   state;

        stack_word_t* sp;
        stack_word_t* stack_begin;
        stack_word_t* stack_end;

        thread_queue_node_t ready;
        thread_queue_node_t block_sleep;
        wait_queue_node_t   waiter;

        tick_time_t deadline {};

        using id_layout = thread_id_layout_t<id_t, build_config::artos::max_threads>;

        id_t id { id_layout::initial_id };

        constexpr id_t make_thread_id(std::size_t index) noexcept
        {
            id = id_layout::next(id, index);
            return id;
        }
    };

    template<bool enabled>
    struct priority_aging_state_t
    {
    };

    template<>
    struct priority_aging_state_t<true>
    {
        priority_t original_priority {};
        tick_t     ticks_at_priority {};
    };

    template<typename Scheduler, bool aging_enabled>
    struct tcb_inst_t
        : tcb_base_t
        , priority_aging_state_t<aging_enabled>
    {
        static constexpr bool priority_aging_enabled = aging_enabled;

        tcb_inst_t(std::string_view thread_name,
                   priority_t       thread_priority,
                   stack_word_t*    stack_pointer,
                   stack_word_t*    stack_start,
                   std::size_t      stack_word_count) noexcept
            : tcb_base_t { thread_name, thread_priority, stack_pointer, stack_start, stack_word_count }
        {
            if constexpr (aging_enabled)
            {
                this->original_priority = thread_priority;
            }
        }

        void age() noexcept
        {
            if constexpr (aging_enabled)
            {
                ++this->ticks_at_priority;
                if (build_config::artos::scheduler.priority_aging_ticks.value() <= this->ticks_at_priority)
                {
                    if (this->priority != priority_t::high)
                    {
                        const auto next_priority = std::to_underlying(this->priority) - 1U;
                        set_effective_priority(this, static_cast<priority_t>(next_priority));
                    }

                    this->ticks_at_priority = 0;
                }
            }
        }

        void reset_priority() noexcept
        {
            if constexpr (aging_enabled)
            {
                set_effective_priority(this, this->original_priority);
                this->ticks_at_priority = 0;
            }
        }
    };

    using tcb_t = tcb_inst_t<decltype(build_config::artos::scheduler),
                             build_config::artos::scheduler.priority_aging_ticks.has_value()>;

#ifndef ARTOS_HOST_TEST
    static_assert(sizeof(tcb_t) == tcb_size);
    static_assert(alignof(tcb_t) <= tcb_alignment);
#endif
}
