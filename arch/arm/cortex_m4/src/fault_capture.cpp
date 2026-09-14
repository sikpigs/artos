#include <fault_capture.h>

extern "C"
{
    volatile fault::record fault_record {};
}

namespace {
    std::uint32_t read_register(std::uintptr_t address)
    {
        return *reinterpret_cast<volatile const std::uint32_t*>(address);
    }
}

extern "C" [[noreturn]] void fault_capture(std::uint32_t sp,
                                           std::uint32_t exc_return,
                                           std::uint32_t msp,
                                           std::uint32_t psp)
{
    fault_record.complete = 0;
    std::uint32_t exception;
    asm volatile("mrs %0, ipsr" : "=r"(exception));
    fault_record.exception = exception;
    fault_record.exc_return = exc_return;
    fault_record.msp = msp;
    fault_record.psp = psp;
    const auto cfsr = read_register(0xE000ED28U);
    fault_record.cfsr = cfsr;
    fault_record.hfsr = read_register(0xE000ED2CU);
    fault_record.dfsr = read_register(0xE000ED30U);
    fault_record.afsr = read_register(0xE000ED3CU);
    fault_record.mmfar = read_register(0xE000ED34U);
    fault_record.bfar = read_register(0xE000ED38U);
    fault_record.shcsr = read_register(0xE000ED24U);
    fault_record.frame_valid = fault::frame_readable(sp, cfsr) ? 1U : 0U;
    if (fault_record.frame_valid != 0)
    {
        const auto* frame = reinterpret_cast<volatile const fault::core_frame*>(sp);
        fault_record.frame.r0 = frame->r0;
        fault_record.frame.r1 = frame->r1;
        fault_record.frame.r2 = frame->r2;
        fault_record.frame.r3 = frame->r3;
        fault_record.frame.r12 = frame->r12;
        fault_record.frame.lr = frame->lr;
        fault_record.frame.pc = frame->pc;
        fault_record.frame.xpsr = frame->xpsr;
    }
    fault_record.complete = 1;
    asm volatile("dsb" ::: "memory");

    // BKPT without a debugger can cause another fault. Poll so a later attach
    // can also stop here. Recovery requires a reset, not exception return.
    for (;;)
    {
        if ((read_register(0xE000EDF0U) & 1U) != 0)
            asm volatile("bkpt #0");
    }
}
