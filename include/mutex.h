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
        success,          // The mutex was acquired or released.
        timeout,          // Acquisition could not complete within its wait budget.
        no_permission,    // A thread other than the owner attempted release.
        not_locked,       // Release found no owner holding the mutex.
        unsupported,      // A mutex operation was called from an ISR.
        deadlock_detected // Blocking acquisition would create an ownership cycle.
    };

    // A nonrecursive mutex owned by the thread that acquires it. Only that
    // thread may release it. Blocked waiters donate priority to the owner;
    // donation propagates through chains of owners waiting on other mutexes.
    // All operations require a running non-idle thread and reject ISR context.
    class mutex_t
    {
    public:
        // Creates an unlocked mutex with no owner or waiting threads.
        mutex_t() noexcept = default;

        // Destruction does not release or cancel waiters. Stop new users,
        // release ownership, and finish existing calls before destruction.
        ~mutex_t() noexcept = default;
        mutex_t(const mutex_t&) = delete;
        mutex_t& operator=(const mutex_t&) = delete;

        // Attempts acquisition without blocking or requesting scheduling.
        // Returns success, timeout if held (including by this thread), or
        // unsupported in an ISR. Does not perform deadlock detection or donate
        // priority when acquisition fails.
        mutex_result_t try_lock() noexcept;

        // Releases ownership, updates the former owner's inherited priority,
        // and wakes a waiting thread to retry acquisition. Requests scheduling
        // if it wakes a higher-priority thread. Returns success, not_locked if
        // unlocked, no_permission if owned by another thread, or unsupported
        // in an ISR.
        mutex_result_t release() noexcept;

        // Acquires using one wait budget across all retries. Zero attempts
        // immediately and returns timeout if held. Before blocking, checks
        // whether waiting would create an ownership cycle, including locking
        // a mutex already owned by this thread. An exhausted wait budget takes
        // precedence over deadlock detection.
        // Returns success, timeout, deadlock_detected, or unsupported in an ISR.
        // If the call can block, do not hold an outer interrupt mask or
        // scheduler lock preventing other threads from running.
        mutex_result_t lock(tick_t ticks_to_wait) noexcept;

        // Waits without a deadline, checking for deadlock before blocking.
        // Uses the same context and blocking restrictions as timed lock.
        // Returns success or deadlock_detected; a wait ending without a signal
        // returns timeout. ISR calls return unsupported.
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
