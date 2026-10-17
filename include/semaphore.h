#pragma once

#include <cassert>
#include <cstdint>

#include <artos.h>
#include <wait_queue.h>

namespace artos {
    enum class semaphore_result_t : std::uint8_t
    {
        success,    // A permit was acquired or the release was accepted.
        timeout,    // Acquisition could not complete within its wait budget.
        full,       // try_acquire found all permits in use.
        closed,     // Closure prevented the operation from completing.
        unsupported // A blocking acquire overload was called from an ISR.
    };

    // Limits the number of permits in use. Acquiring increments that count;
    // releasing decrements it. Permits have no thread ownership, so another
    // thread or an ISR may release them. There is no mutex priority donation.
    class semaphore_t
    {
        enum class state_t : std::uint8_t
        {
            open,
            closed
        };

    public:
        using count_t = std::uint32_t;

        // Creates an open semaphore. maximum_count must be positive and
        // initial_count must not exceed it. initial_count counts permits
        // already in use: zero makes all permits available; maximum_count
        // makes none available until a release.
        explicit semaphore_t(count_t maximum_count, count_t initial_count = 0) noexcept
            : maximum { maximum_count }
            , count { initial_count }
            , state { state_t::open }
        {
            assert(maximum_count > 0UL && initial_count <= maximum_count);
        }

        // Destruction does not close or wait. Stop new users and finish all
        // existing calls before destroying the semaphore.
        ~semaphore_t() noexcept = default;

        semaphore_t(const semaphore_t&) = delete;
        semaphore_t& operator=(const semaphore_t&) = delete;

        // Attempts acquisition without blocking or requesting scheduling.
        // Returns success, full, or closed. May run in thread context or an ISR
        // masked by the kernel's BASEPRI threshold: its encoded priority must
        // be numerically >= port::kernel_interrupt_priority. Higher-priority
        // ISRs and NMI/fault handlers must defer kernel operations.
        semaphore_result_t try_acquire() noexcept;

        // Acquires from a running non-idle thread, using one wait budget across
        // all retries. Zero attempts immediately and returns timeout if full.
        // Returns success, timeout, or closed; ISR calls return unsupported,
        // even with a zero budget. If the call can block, do not hold an outer
        // interrupt mask or scheduler lock preventing other threads from running.
        semaphore_result_t acquire(tick_t ticks_to_wait) noexcept;

        // Waits until a permit can be acquired or the semaphore closes.
        // Uses the same thread/context restrictions as timed acquire.
        // Returns success or closed; ISR calls return unsupported.
        semaphore_result_t acquire(wait_forever_t) noexcept;

        // Releases one permit from thread context and wakes a waiting acquirer,
        // requesting scheduling if it wakes a higher-priority thread. Returns
        // success or closed. With no permits in use, the count stays zero and
        // the release still succeeds; repeated releases do not add capacity.
        semaphore_result_t release() noexcept;

        // Releases without blocking, using the ISR priority restrictions above.
        // Returns success or closed, with the same count behavior as release.
        // If supplied, the flag is only set to true when a higher-priority
        // thread is woken; an existing true is never cleared. Initialize it to
        // false before a batch, then request scheduling if true before leaving
        // the ISR. nullptr discards the indication but still wakes the waiter.
        semaphore_result_t release_from_isr(bool* higher_priority_thread_woken = nullptr) noexcept;

        // Permanently closes and cancels blocked acquisitions. Idempotent and
        // nonblocking; later acquire/release calls return closed. Cancelled
        // calls may still be returning. Does not request deletion.
        void close() noexcept;

        // Closes and waits for operations that entered while open to exit.
        // Call from a running non-idle thread with no mask or scheduler lock
        // preventing their completion. Stop new users first, including ISR
        // users. This does not wait for permit holders' work or future releases,
        // and does not delete the semaphore. Do not combine with a concurrent
        // close_and_delete request.
        void close_and_wait() noexcept;

        // Closes and requests deferred self-deletion, including if already
        // closed. Deletes immediately when idle; otherwise returns without
        // waiting and the last active operation deletes it as it exits.
        // Requires an exclusively owned object allocated with new semaphore_t.
        // Call from thread context after stopping new users, including ISR
        // users, and arrange for existing calls to finish. Do not access the
        // pointer afterward, delete it separately, or have concurrent lifecycle
        // callers. Outstanding permits do not keep the object alive.
        void close_and_delete() noexcept;

    private:
        class operation_guard_t
        {
        public:
            explicit operation_guard_t(semaphore_t& owner) noexcept;
            ~operation_guard_t() noexcept;

            operation_guard_t(const operation_guard_t&) = delete;
            operation_guard_t& operator=(const operation_guard_t&) = delete;

            [[nodiscard]] bool entered() const noexcept
            {
                return active;
            }

        private:
            thread::internal::operation_cleanup_t cleanup {};
            bool                                  active { false };
        };

        void               finish_operation() noexcept;
        semaphore_result_t release(const operation_guard_t& operation) noexcept;
        semaphore_result_t acquire(std::optional<tick_t> ticks_to_wait) noexcept;

        count_t     maximum;
        count_t     count;
        state_t     state { state_t::closed };
        bool        delete_on_close { false };
        std::size_t active_operations { 0 };

        wait_queue_t waiters {};
        wait_queue_t close_queue {};
    };
}
