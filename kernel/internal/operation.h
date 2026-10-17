#pragma once

#include <cassert>
#include <utility>

#include <artos.h>
#include <internal/kernel_state.h>
#include <internal/thread.h>

namespace artos::thread::internal {
    // Call while masked, in the same critical section as incrementing the
    // object's operation count. ISR operations finish before task execution
    // resumes, so they must not be registered against the interrupted task.
    inline void register_operation(operation_cleanup_t& operation,
                                   void*                context,
                                   void (*finish)(void*) noexcept) noexcept
    {
        assert(operation.finish == nullptr);
        operation.context = context;
        operation.finish = finish;
        if (!cpu::port::is_in_irq() && kernel::internal::current_thread)
        {
            auto* thread = kernel::internal::current_thread;
            operation.thread = thread;
            operation.next = thread->active_operations;
            thread->active_operations = &operation;
        }
    }

    inline void complete_operation(operation_cleanup_t& operation) noexcept
    {
        const critical_section_t cs;
        if (operation.thread)
        {
            auto** link = &operation.thread->active_operations;
            while (*link != &operation)
            {
                assert(*link);
                link = &(*link)->next;
            }
            *link = operation.next;
            operation.thread = nullptr;
            operation.next = nullptr;
        }

        // Unlink before invoking cleanup: it may delete its owner and request
        // scheduling. Never dereference context after this callback.
        const auto finish = std::exchange(operation.finish, nullptr);
        auto*      context = std::exchange(operation.context, nullptr);
        if (finish)
        {
            finish(context);
        }
    }
}
