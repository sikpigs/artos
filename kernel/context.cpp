#include <internal/kernel_state.h>
#include <internal/thread.h>

#include <artos.h>

extern "C"
{
    using namespace artos::thread;
    using namespace artos::kernel::internal;

    bool artos_context_started() noexcept
    {
        return current_thread != nullptr;
    }

    stack_word_t* artos_context_start() noexcept
    {
        if (current_thread != nullptr)
        {
            reinterpret_cast<internal::tcb_t*>(current_thread)->post_run();
        }
        current_thread = next_thread;
        next_thread = nullptr;
        current_thread->state = state_t::running;
        reinterpret_cast<internal::tcb_t*>(current_thread)->pre_run();

        return current_thread->sp;
    }

    stack_word_t* artos_context_switch(stack_word_t* outgoing_sp) noexcept
    {
        if (current_thread != nullptr)
        {
            current_thread->sp = outgoing_sp;
            if (current_thread->state == state_t::running)
            {
                current_thread->state = state_t::ready;
            }
        }

        return artos_context_start();
    }
}