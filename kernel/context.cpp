#include <internal/thread.h>
#include <internal/thread_state.h>

extern "C"
{
    using namespace artos::thread;
    using namespace artos::thread::internal;

    bool artos_context_started() noexcept
    {
        return current_thread != nullptr;
    }

    stack_word_t* artos_context_start() noexcept
    {
        current_thread = next_thread;
        next_thread = nullptr;
        current_thread->state = thread_state_t::running;
        return current_thread->sp;
    }

    stack_word_t* artos_context_switch(stack_word_t* outgoing_sp) noexcept
    {
        if (current_thread != nullptr)
        {
            current_thread->sp = outgoing_sp;
            if (current_thread->state == thread_state_t::running)
            {
                current_thread->state = thread_state_t::ready;
            }
        }

        current_thread = next_thread;
        current_thread->state = thread_state_t::running;
        next_thread = nullptr;

        return current_thread->sp;
    }
}