#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include <artos.h>
#include <wait_queue.h>

namespace artos {
    // Caller-owned storage for Count elements. Keep it alive until the queue
    // and all of its users have finished; do not share it between live queues.
    template<typename Type, std::size_t Count>
    requires(Count > 0 && std::is_object_v<Type> && std::is_trivially_copyable_v<Type> && !std::is_const_v<Type>
             && !std::is_volatile_v<Type>)
    struct queue_storage_t
    {
        static constexpr std::size_t count = Count;
        static constexpr std::size_t elem_size = sizeof(Type);
        static constexpr std::size_t buffer_size = elem_size * count;

        alignas(alignof(Type)) std::array<std::byte, buffer_size> buffer;
    };

    enum class queue_result_t : std::uint8_t
    {
        success, // The element was inserted or removed.
        timeout, // A timed operation could not complete within its wait budget.
        full,    // A try_push operation found no free slot.
        empty,   // A try_pop operation found no element.
        closed   // The queue was closed before the operation completed.
    };

    namespace internal {

        class queue_core_t
        {
            enum class state_t : std::uint8_t
            {
                open,
                closed
            };

        public:
            virtual ~queue_core_t() noexcept = default;

            queue_core_t(const queue_core_t&) = delete;
            queue_core_t& operator=(const queue_core_t&) = delete;

        protected:
            queue_core_t(std::size_t count, std::size_t element_size, std::byte* storage) noexcept
                : capacity { count }
                , elem_size { element_size }
                , buffer { storage }
                , state { state_t::open }
            {
            }

            [[nodiscard]] queue_result_t push_front(const void* element, std::optional<tick_t> ticks_to_wait) noexcept;
            [[nodiscard]] queue_result_t push_back(const void* element, std::optional<tick_t> ticks_to_wait) noexcept;
            [[nodiscard]] queue_result_t pop_front(void* element, std::optional<tick_t> ticks_to_wait) noexcept;
            [[nodiscard]] queue_result_t pop_back(void* element, std::optional<tick_t> ticks_to_wait) noexcept;

            [[nodiscard]] queue_result_t try_push_front(const void* element,
                                                        bool*       higher_priority_thread_woken) noexcept;
            [[nodiscard]] queue_result_t try_push_back(const void* element,
                                                       bool*       higher_priority_thread_woken) noexcept;
            [[nodiscard]] queue_result_t try_pop_front(void* element, bool* higher_priority_thread_woken) noexcept;
            [[nodiscard]] queue_result_t try_pop_back(void* element, bool* higher_priority_thread_woken) noexcept;

            [[nodiscard]] bool is_full() const noexcept
            {
                return capacity == size;
            }

            [[nodiscard]] bool is_empty() const noexcept
            {
                return size == 0;
            }

            void close() noexcept;
            void close_and_wait() noexcept;
            void close_and_delete() noexcept;

        private:
            class operation_guard_t
            {
            public:
                explicit operation_guard_t(queue_core_t& owner) noexcept;
                ~operation_guard_t() noexcept;

                operation_guard_t(const operation_guard_t&) = delete;
                operation_guard_t& operator=(const operation_guard_t&) = delete;

                [[nodiscard]] bool entered() const noexcept
                {
                    return active;
                }

            private:
                queue_core_t& queue;
                bool          active { false };
            };

            void finish_operation() noexcept;

            std::size_t capacity { 0 };
            std::size_t elem_size { 0 };
            std::size_t size { 0 };
            std::byte*  buffer { nullptr };
            std::size_t head { 0 };
            std::size_t tail { 0 };
            state_t     state { state_t::closed };
            bool        delete_on_close { false };
            std::size_t active_operations { 0 };

            wait_queue_t reader_queue {};
            wait_queue_t writer_queue {};
            wait_queue_t close_queue {};
        };
    }

    // A bounded deque that copies trivially copyable elements into external
    // storage. push_back/pop_front provide FIFO order; either end may be used.
    // Element references must remain valid throughout the call. A failed pop
    // leaves its output unchanged; a failed push does not insert an element.
    template<typename Type>
    class queue_t : private internal::queue_core_t
    {
    public:
        using super = internal::queue_core_t;

        // Creates an empty, open queue with capacity Count. Storage must outlive
        // the queue and all calls using it.
        template<std::size_t Count>
        explicit queue_t(queue_storage_t<Type, Count>& storage) noexcept
            : queue_core_t(Count, sizeof(Type), storage.buffer.data())
        {
        }

        // Destruction does not close or wait. Stop new users and finish all
        // existing calls before destroying the queue or its storage.
        ~queue_t() noexcept = default;

        queue_t(const queue_t&) = delete;
        queue_t& operator=(const queue_t&) = delete;

        // The push/pop overloads are for a running non-idle thread. If a call
        // can block, do not hold an outer interrupt mask or scheduler lock
        // that prevents other threads from running. They request scheduling
        // when they wake a higher-priority thread.
        //
        // wait_forever waits for space (push) or an element (pop) until the
        // operation succeeds or the queue closes. Returns success or closed.
        [[nodiscard]] queue_result_t push_front(const Type& element, wait_forever_t) noexcept
        {
            return super::push_front(&element, std::nullopt);
        }

        [[nodiscard]] queue_result_t push_back(const Type& element, wait_forever_t) noexcept
        {
            return super::push_back(&element, std::nullopt);
        }

        [[nodiscard]] queue_result_t pop_front(Type& element, wait_forever_t) noexcept
        {
            return super::pop_front(&element, std::nullopt);
        }

        [[nodiscard]] queue_result_t pop_back(Type& element, wait_forever_t) noexcept
        {
            return super::pop_back(&element, std::nullopt);
        }

        // Timed overloads use one wait budget across all retries. A zero budget
        // attempts the operation immediately and returns timeout if unavailable.
        // Returns success, timeout, or closed; closure prevents access even to
        // elements already buffered in the queue.
        [[nodiscard]] queue_result_t push_front(const Type& element, tick_t ticks_to_wait) noexcept
        {
            return super::push_front(&element, ticks_to_wait);
        }

        [[nodiscard]] queue_result_t push_back(const Type& element, tick_t ticks_to_wait) noexcept
        {
            return super::push_back(&element, ticks_to_wait);
        }

        [[nodiscard]] queue_result_t pop_front(Type& element, tick_t ticks_to_wait) noexcept
        {
            return super::pop_front(&element, ticks_to_wait);
        }

        [[nodiscard]] queue_result_t pop_back(Type& element, tick_t ticks_to_wait) noexcept
        {
            return super::pop_back(&element, ticks_to_wait);
        }

        // try_* operations never block or request scheduling. They may run in
        // thread context or in an ISR masked by the kernel's BASEPRI threshold:
        // its encoded priority must be numerically >= port::kernel_interrupt_priority.
        // Higher-priority ISRs and NMI/fault handlers must defer the operation.
        //
        // If supplied, higher_priority_thread_woken is only set to true when a
        // higher-priority thread is woken; an existing true is never cleared.
        // Initialize it to false before a batch of calls, then request scheduling
        // if it becomes true (before returning from an ISR). nullptr discards
        // the indication but still wakes the waiter; scheduling occurs when the
        // scheduler next runs. These calls require the kernel to be running.
        //
        // Inserts at the requested end. Returns success, full, or closed.
        [[nodiscard]] queue_result_t try_push_front(const Type& element,
                                                    bool*       higher_priority_thread_woken = nullptr) noexcept
        {
            return super::try_push_front(&element, higher_priority_thread_woken);
        }

        [[nodiscard]] queue_result_t try_push_back(const Type& element,
                                                   bool*       higher_priority_thread_woken = nullptr) noexcept
        {
            return super::try_push_back(&element, higher_priority_thread_woken);
        }

        // Removes from the requested end. Returns success, empty, or closed.
        [[nodiscard]] queue_result_t try_pop_front(Type& element, bool* higher_priority_thread_woken = nullptr) noexcept
        {
            return super::try_pop_front(&element, higher_priority_thread_woken);
        }

        [[nodiscard]] queue_result_t try_pop_back(Type& element, bool* higher_priority_thread_woken = nullptr) noexcept

        {
            return super::try_pop_back(&element, higher_priority_thread_woken);
        }

        // Permanently closes the queue and cancels blocked readers and writers.
        // Idempotent and nonblocking; later push/pop calls return closed, even
        // if buffered elements remain. Cancelled calls may still be returning.
        void close() noexcept
        {
            super::close();
        }

        // Closes and waits for operations that entered while open to exit.
        // Call from a running non-idle thread with no mask or scheduler lock
        // preventing their completion. Stop new users first, including ISR
        // producers, and finish users holding references before destruction;
        // this does not wait for work performed by callers after a call returns.
        void close_and_wait() noexcept
        {
            super::close_and_wait();
        }

        void close_and_delete() noexcept
        {
            super::close_and_delete();
        }
    };
}
