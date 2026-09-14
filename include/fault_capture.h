#pragma once

#include <cstdint>
#include <mcu.h>

namespace fault {
    struct core_frame
    {
        std::uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;
    };

    struct record
    {
        std::uint32_t complete;
        std::uint32_t exception, exc_return, msp, psp;
        std::uint32_t cfsr, hfsr, dfsr, afsr, mmfar, bfar, shcsr;
        std::uint32_t frame_valid;
        core_frame    frame;
    };

    // Check before touching a possibly corrupted exception stack. Core registers
    // start at SP for both basic and FP-extended Cortex-M4 exception frames.
    constexpr bool frame_readable(std::uint32_t sp, std::uint32_t cfsr)
    {
        constexpr auto stacking_errors = (1U << 4) | (1U << 5) | (1U << 12) | (1U << 13);
        return (cfsr & stacking_errors) == 0 && (sp & 3U) == 0 && mcu::readable_ram(sp, sizeof(core_frame));
    }
}

// Inspect with `p/x fault_record` in GDB. Cleared on reset; retained while halted.
extern "C" volatile fault::record fault_record;
