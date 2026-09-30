#pragma once

#include <build_config.h>

#include <cstddef>
#include <cstdint>

namespace artos::thread {
    enum class stack_overflow_phase_t : std::uint32_t
    {
        outgoing = 1,
        incoming = 2
    };

    struct stack_overflow_record_t
    {
        std::uint32_t          complete;
        std::uint32_t          reason; // 1 = thread stack canary damaged
        stack_overflow_phase_t phase;
        std::uintptr_t         tcb_address;
        std::uintptr_t         thread_id; // Best effort: the TCB may already be damaged.
        std::size_t            slot;
    };
}

// Inspect with `p/x stack_overflow_record` in GDB. Cleared on reset.
#ifdef CONFIG_ARTOS_THREAD_STACK_CANARY
extern "C" volatile artos::thread::stack_overflow_record_t stack_overflow_record;
#endif
