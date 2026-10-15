#include <internal/kernel_state.h>
#include <internal/thread.h>

#include <artos.h>

#ifdef CONFIG_ARTOS_THREAD_STACK_CANARY
#include <port/cpu_port.h>
#include <stack_overflow.h>

extern "C"
{
    volatile artos::thread::stack_overflow_record_t stack_overflow_record {};
}

namespace {
    [[noreturn]] void record_stack_overflow(artos::thread::internal::tcb_t*       thread,
                                            artos::thread::stack_overflow_phase_t phase) noexcept
    {
        using namespace artos::kernel::internal;
        (void)cpu::port::irq_save_disable();

        stack_overflow_record.complete = 0;
        stack_overflow_record.reason = 1;
        stack_overflow_record.phase = phase;
        stack_overflow_record.tcb_address = reinterpret_cast<std::uintptr_t>(thread);
        stack_overflow_record.slot = static_cast<std::size_t>(-1);
        for (std::size_t index = 0; index < threads.size(); ++index)
        {
            if (threads[index] == thread)
            {
                stack_overflow_record.slot = index;
                break;
            }
        }
        stack_overflow_record.thread_id = thread->id;
        stack_overflow_record.complete = 1;
        __asm volatile("dsb" ::: "memory");
        cpu::port::fatal_trap();
    }
}
#endif

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
#ifdef CONFIG_ARTOS_THREAD_STACK_CANARY
        auto* incoming = reinterpret_cast<internal::tcb_t*>(next_thread);
        if (!incoming->canary_intact())
        {
            record_stack_overflow(incoming, stack_overflow_phase_t::incoming);
        }
#endif
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
#ifdef CONFIG_ARTOS_THREAD_STACK_CANARY
            auto* outgoing = reinterpret_cast<internal::tcb_t*>(current_thread);
            if (!outgoing->canary_intact())
            {
                record_stack_overflow(outgoing, stack_overflow_phase_t::outgoing);
            }
#endif
            current_thread->sp = outgoing_sp;
            if (current_thread->state == state_t::running)
            {
                current_thread->state = state_t::ready;
            }
        }

        return artos_context_start();
    }
}
