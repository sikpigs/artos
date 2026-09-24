#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

#include <artos.h>
#include <wait_queue.h>

namespace artos {
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

    namespace internal {

        class queue_core_t
        {
        public:
            virtual ~queue_core_t() noexcept = default;

            queue_core_t(const queue_core_t&) = delete;
            queue_core_t& operator=(const queue_core_t&) = delete;

        protected:
            queue_core_t(std::size_t count, std::size_t element_size, std::byte* storage) noexcept
                : capacity { count }
                , elem_size { element_size }
                , buffer { storage }
            {
            }

            [[nodiscard]] bool push_front(const void* element, tick_t ticks_to_wait) noexcept;
            [[nodiscard]] bool push_back(const void* element, tick_t ticks_to_wait) noexcept;
            [[nodiscard]] bool pop_front(void* element, tick_t ticks_to_wait) noexcept;
            [[nodiscard]] bool pop_back(void* element, tick_t ticks_to_wait) noexcept;

            [[nodiscard]] bool is_full() const noexcept
            {
                return capacity == size;
            }

            [[nodiscard]] bool is_empty() const noexcept
            {
                return size == 0;
            }

        private:
            std::size_t capacity { 0 };
            std::size_t elem_size { 0 };
            std::size_t size { 0 };
            std::byte*  buffer { nullptr };
            std::size_t head { 0 };
            std::size_t tail { 0 };

            wait_queue_t reader_queue {};
            wait_queue_t writer_queue {};
        };
    }

    template<typename Type>
    class queue_t : private internal::queue_core_t
    {
    public:
        using super = internal::queue_core_t;

        template<std::size_t Count>
        explicit queue_t(queue_storage_t<Type, Count>& storage) noexcept
            : queue_core_t(Count, sizeof(Type), storage.buffer.data())
        {
        }

        ~queue_t() noexcept = default;

        queue_t(const queue_t&) = delete;
        queue_t& operator=(const queue_t&) = delete;

        [[nodiscard]] bool push_front(const Type& element, tick_t ticks_to_wait) noexcept
        {
            return super::push_front(&element, ticks_to_wait);
        }

        [[nodiscard]] bool push_back(const Type& element, tick_t ticks_to_wait) noexcept
        {
            return super::push_back(&element, ticks_to_wait);
        }

        [[nodiscard]] bool pop_front(Type& element, tick_t ticks_to_wait) noexcept
        {
            return super::pop_front(&element, ticks_to_wait);
        }

        [[nodiscard]] bool pop_back(Type& element, tick_t ticks_to_wait) noexcept
        {
            return super::pop_back(&element, ticks_to_wait);
        }
    };
}
