#include <atomic>

#include <artos.h>
#include <artos_port.h>

namespace artos::kernel {
    namespace internal {
        std::atomic<tick_t> ticks { 0 };
        static_assert(std::atomic<tick_t>::is_always_lock_free);

        void on_tick() noexcept
        {
            ticks.fetch_add(1, std::memory_order_relaxed);
        }
    }

    kernel_t::kernel_t() noexcept
    {
        internal::ticks.store(0, std::memory_order_relaxed);
        port::start_tick();
    }
}

namespace artos::thread {
    namespace internal {
        enum class thread_state_t : std::uint8_t
        {
            inactive,
            ready,
            running,
            sleeping,
            finished
        };

        struct tcb_t
        {
            stack_word_t* sp;
            stack_word_t* stack_begin;
            stack_word_t* stack_end;

            std::string_view name;
            priority_t       priority;
            thread_state_t   state = thread_state_t::inactive;
            std::uint32_t    wake_tick = 0;
        };

        static_assert(sizeof(tcb_t) == tcb_size);
        static_assert(alignof(tcb_t) <= tcb_alignment);

        start_result_t create(func_t              func,
                              std::string_view    name,
                              std::uint32_t       stack_size_bytes,
                              void*               parameters,
                              priority_t          priority,
                              std::byte* const    tcb,
                              stack_word_t* const stack)
        {
            auto*      task = reinterpret_cast<tcb_t*>(tcb);
            const auto stack_word_count = stack_size_bytes / sizeof(stack_word_t);
            auto*      sp = port::initialize_stack(stack + stack_word_count, func, parameters);

            task->sp = sp;
            task->stack_begin = sp;
            task->stack_end = stack;

            task->name = name;
            task->priority = priority;
            task->state = thread_state_t::inactive;
            task->wake_tick = 0;

            return 0;
        }
    }
}
