#pragma once

#include <board_services.h>
#include <build_config.h>

#include <internal/kernel_state.h>
#include <internal/thread_id.h>
#include <internal/thread_queue.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <utility>

#include <artos.h>
#include <intrusive_list.h>
#include <wait_queue.h>

namespace artos::thread::internal {
    inline constexpr stack_word_t stack_fill_pattern = 0xA5A5A5A5u;

    struct tcb_base_t;

    void set_priority_ready(priority_t priority) noexcept;
    void clear_priority_ready(priority_t priority) noexcept;

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
            : priority { thread_priority }
            , state { state_t::inactive }
            , sp { stack_pointer }
            , stack_begin { stack_start }
            , stack_end { stack_start + stack_word_count }
            , ready { this }
            , block_sleep { this }
            , waiter { this }
        {
            if (!thread_name.empty())
            {
                std::memcpy(name.data(), thread_name.data(), thread_name.size());
            }
            name[thread_name.size()] = '\0';
        }

        tcb_base_t(const tcb_base_t&) = delete;
        tcb_base_t& operator=(const tcb_base_t&) = delete;

        ~tcb_base_t() noexcept = default;

        name_t     name {};
        priority_t priority;
        state_t    state;

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

        func_t on_exit;
        void*  on_exit_ctx;
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

    template<bool enabled>
    struct high_watermark_t
    {
    };

    template<>
    struct high_watermark_t<true>
    {
        std::size_t max_observed_bytes {};
    };

    template<bool enabled>
    struct thread_stats_t
    {
    };

    template<>
    struct thread_stats_t<true>
    {
        statistics_t statistics {};
        uint32_t     execution_start {};
    };

    template<typename Scheduler, bool aging_enabled, bool hwm_enabled, bool stats_enabled>
    struct tcb_inst_t
        : tcb_base_t
        , priority_aging_state_t<aging_enabled>
        , high_watermark_t<hwm_enabled>
        , thread_stats_t<stats_enabled>
    {
        static constexpr bool priority_aging_enabled = aging_enabled;
        static constexpr bool high_watermark_enabled = hwm_enabled;
        static constexpr bool statistics_enabled = stats_enabled;

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
            if constexpr (priority_aging_enabled)
            {
                set_effective_priority(this, this->original_priority);
                this->ticks_at_priority = 0;
            }
        }

        void clear_runtime_history_bucket(std::size_t bucket_index) noexcept
        {
            if constexpr (statistics_enabled)
            {
                this->statistics.runtime_history_cycles[bucket_index] = 0;
            }
        }

        void pre_run() noexcept
        {
            if constexpr (statistics_enabled)
            {
                ++this->statistics.execution_count;
                const auto now = board::stopwatch::count();
                this->execution_start = now;
            }
        }

        void post_run() noexcept
        {
            if constexpr (statistics_enabled)
            {
                const auto now = board::stopwatch::count();
                this->statistics.runtime_history_cycles[kernel::internal::runtime_history_bucket_index]
                    += now - this->execution_start;
            }
        }

        void keep_running([[maybe_unused]] std::uint32_t now) noexcept
        {
            if constexpr (statistics_enabled)
            {
                this->statistics.runtime_history_cycles[kernel::internal::runtime_history_bucket_index]
                    += now - this->execution_start;
                this->execution_start = now;
            }
        }

        void keep_running() noexcept
        {
            if constexpr (statistics_enabled)
            {
                keep_running(board::stopwatch::count());
            }
        }

        [[nodiscard]] std::size_t get_high_watermark() noexcept
        {
            if constexpr (high_watermark_enabled)
            {
                stack_word_t* hwm = this->stack_begin;
                while (hwm < this->stack_end && *hwm == stack_fill_pattern)
                {
                    ++hwm;
                }

                const auto used_bytes = static_cast<std::size_t>(this->stack_end - hwm) * sizeof(stack_word_t);
                this->max_observed_bytes = std::max(this->max_observed_bytes, used_bytes);

                return this->max_observed_bytes;
            }
            else
            {
                return 0;
            }
        }

        void get_statistics(statistics_t& stats) noexcept
        {
            using namespace kernel::internal;
            if constexpr (statistics_enabled)
            {
                std::memcpy(std::addressof(stats), &this->statistics, sizeof(stats));
                if (this == current_thread && this->state == state_t::running)
                {
                    stats.runtime_history_cycles[runtime_history_bucket_index]
                        += board::stopwatch::count() - this->execution_start;
                }
            }
        }
    };

    using tcb_t = tcb_inst_t<decltype(build_config::artos::scheduler),
                             build_config::artos::scheduler.priority_aging_ticks.has_value(),
                             build_config::artos::thread_stack_hwm,
                             build_config::artos::thread_stats>;

#ifndef ARTOS_HOST_TEST
    static_assert(sizeof(tcb_t) == tcb_size);
    static_assert(alignof(tcb_t) <= tcb_alignment);
#endif
}
