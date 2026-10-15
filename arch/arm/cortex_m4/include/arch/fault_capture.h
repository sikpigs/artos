#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fault {
    void set_active_stack_guard(std::uintptr_t begin, std::uintptr_t end) noexcept;

    struct core_frame_t
    {
        std::uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;
    };

    struct fpu_frame_t
    {
        core_frame_t                  core_frame;
        std::array<std::uint32_t, 18> fpu_registers;
    };

    static_assert(sizeof(core_frame_t) == 32);
    static_assert(sizeof(fpu_frame_t) == 104);
    static_assert(offsetof(fpu_frame_t, core_frame) == 0);

    struct record_t

    {
        std::uint32_t complete;
        std::uint32_t exception, exc_return, msp, psp;
        std::uint32_t cfsr, hfsr, dfsr, afsr, mmfar, bfar, shcsr;

        std::uint32_t frame_valid;
        core_frame_t  frame;
    };
}

// Inspect with `p/x fault_record` in GDB. Cleared on reset; retained while halted.
extern "C" volatile fault::record_t fault_record;
