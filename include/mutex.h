#pragma once

#include <artos.h>
#include <atomic>
#include <cstdint>

#include <artos.h>
#include <intrusive_list.h>
#include <wait_queue.h>

namespace artos {

    class mutex_t;

    namespace thread::internal {
        class mutexes_held_t;
    }

    namespace kernel::internal {
        void prioritize(mutex_t* mutex) noexcept;
        void recalculate_priority(thread::id_t thread_id) noexcept;
    }

    enum class mutex_result_t : std::uint8_t
    {
        success,
        timeout,
        no_permission,
        not_locked,
        unsupported,
        deadlock_detected
    };

    class mutex_t
    {
    public:
        mutex_t() noexcept = default;
        ~mutex_t() noexcept = default;
        mutex_t(const mutex_t&) = delete;
        mutex_t& operator=(const mutex_t&) = delete;

        mutex_result_t try_lock() noexcept;
        mutex_result_t release() noexcept;
        mutex_result_t lock(tick_t ticks_to_wait) noexcept;
        mutex_result_t lock(wait_forever_t) noexcept;

    private:
        std::atomic<std::uint_fast32_t> state { 0 };
        thread::id_t                    owner { thread::invalid_id };
        wait_queue_t                    waiters;
        list_hook_t                     hook;

        friend void kernel::internal::prioritize(mutex_t* mutex) noexcept;
        friend void kernel::internal::recalculate_priority(thread::id_t thread_id) noexcept;
        friend class thread::internal::mutexes_held_t;

        mutex_result_t lock(std::optional<tick_t> ticks_to_wait) noexcept;

        void unhook() noexcept;
        void prioritize() noexcept;
    };
}