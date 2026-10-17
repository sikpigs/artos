#pragma once

#include <algorithm>
#include <cstring>
#include <optional>
#include <utility>

#include <board_services.h>
#include <build_config.h>

#include <artos.h>
#include <event.h>
#include <intrusive_list.h>
#include <mutex.h>
#include <wait_queue.h>

#include <port/artos_port.h>

#include <internal/container_of.h>
#include <internal/kernel_state.h>
#include <internal/thread_id.h>
#include <internal/thread_queue.h>

namespace artos::thread::internal {
    using namespace build_config::artos;

    class mutexes_held_t
    {
    public:
        class iterator_t
        {
        public:
            explicit iterator_t(mutex_t* mutex_node)
                : mutex { mutex_node }
            {
            }

            ~iterator_t() noexcept = default;

            mutex_t& operator*() noexcept
            {
                return *mutex;
            }

            mutex_t* operator->() noexcept
            {
                return mutex;
            }

            bool operator==(const iterator_t& other) const noexcept = default;

            iterator_t operator++() noexcept
            {
                auto next = mutex->hook.next;
                mutex = ARTOS_CONTAINER_OF(next, mutex_t, hook);
                return *this;
            }

        protected:
            mutex_t* mutex;
        };

        void push_back(mutex_t* mutex) noexcept
        {
            list.push_back(mutex->hook);
        }

        [[nodiscard]] bool empty() noexcept
        {
            return list.empty();
        }

        void erase(mutex_t* mutex) noexcept
        {
            list.erase(mutex->hook);
        }

        [[nodiscard]] iterator_t begin() noexcept
        {
            return iterator_t { ARTOS_CONTAINER_OF(list.front(), mutex_t, hook) };
        }

        [[nodiscard]] iterator_t end() noexcept
        {
            return iterator_t { ARTOS_CONTAINER_OF(list.end(), mutex_t, hook) };
        }

    private:
        intrusive_list_t list;
    };

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
            , base_priority { priority }
            , sp { stack_pointer }
            , stack_bottom { stack_start }
            , stack_top { stack_start + stack_word_count }
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
        priority_t base_priority;
        state_t    state { state_t::inactive };
        bool       suspended { false };

        stack_word_t* sp;
        stack_word_t* stack_bottom;
        stack_word_t* stack_top;

        thread_queue_node_t ready;
        thread_queue_node_t block_sleep;
        wait_queue_node_t   waiter;

        wait_queue_t joiners {};
        list_hook_t  join_hook {};

        mutexes_held_t mutexes {};
        mutex_t*       waiting_mutex {};

        event_t*     waiting_event {};
        any_or_all_t waiting_for {};

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
        std::optional<priority_t> aged_priority {};
        tick_t                    ticks_at_priority {};
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
    struct canary_state_t
    {
    };

    template<>
    struct canary_state_t<true>
    {
        stack_word_t* canary_begin {};
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

    template<typename Scheduler, bool aging_enabled, bool hwm_enabled, bool canary_enabled, bool stats_enabled>
    struct tcb_inst_t
        : tcb_base_t
        , priority_aging_state_t<aging_enabled>
        , high_watermark_t<hwm_enabled>
        , canary_state_t<canary_enabled>
        , thread_stats_t<stats_enabled>
    {
        static constexpr bool priority_aging_enabled = aging_enabled;
        static constexpr bool high_watermark_enabled = hwm_enabled;
        static constexpr bool statistics_enabled = stats_enabled;

        tcb_inst_t(std::string_view thread_name,
                   priority_t       thread_priority,
                   stack_word_t*    stack_pointer,
                   stack_word_t*    stack_start,
                   std::size_t      stack_word_count,
                   stack_word_t*    canary = nullptr) noexcept
            : tcb_base_t { thread_name, thread_priority, stack_pointer, stack_start, stack_word_count }
        {
            if constexpr (canary_enabled)
            {
                this->canary_begin = canary;
            }
        }

        [[nodiscard]] bool canary_intact() const noexcept
        {
            if constexpr (canary_enabled)
            {
                const volatile stack_word_t* const words = this->canary_begin;
                for (std::size_t index = 0; index < canary_word_count; ++index)
                {
                    if (words[index] != stack_fill_pattern)
                    {
                        return false;
                    }
                }
            }
            return true;
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
                        const auto next_priority = static_cast<priority_t>(std::to_underlying(this->priority) - 1U);
                        this->aged_priority = next_priority;
                        kernel::internal::recalculate_priority(this->id);
                    }

                    this->ticks_at_priority = 0;
                }
            }
        }

        void reset_priority() noexcept
        {
            if constexpr (priority_aging_enabled)
            {
                if (this->aged_priority)
                {
                    this->aged_priority.reset();
                    kernel::internal::recalculate_priority(this->id);
                }

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

            if constexpr (thread_stack_protection)
            {
                const global_interrupt_guard_t gig {};
                port::activate_stack_protection(
                    { reinterpret_cast<std::byte*>(stack_bottom), reinterpret_cast<std::byte*>(stack_top) });
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
                stack_word_t* hwm = this->stack_bottom;
                while (hwm < this->stack_top && *hwm == stack_fill_pattern)
                {
                    ++hwm;
                }

                const auto used_bytes = static_cast<std::size_t>(this->stack_top - hwm) * sizeof(stack_word_t);
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
                             build_config::artos::thread_stack_canary,
                             build_config::artos::thread_stats>;

    static_assert(sizeof(thread_queue_node_t) == thread_queue_storage_layout.padded_size());
    static_assert(alignof(thread_queue_node_t) == thread_queue_storage_layout.alignment);
    static_assert(sizeof(tcb_t) == tcb_size);
    static_assert(alignof(tcb_t) <= tcb_alignment);

    using idle_thread_storage_t = thread::storage_t<thread::minimum_stack_bytes>;

    [[nodiscard]]
    create_result_t set_idle(func_t func, idle_thread_storage_t& storage) noexcept;
}
