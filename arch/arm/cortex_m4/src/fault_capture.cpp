#include <arch/artos.h>
#include <arch/fault_capture.h>
#include <arch/regmap.h>

#include <cstddef>
#include <fault.h>
#include <mcu.h>

using namespace artos::port;
using namespace fault;

namespace fault {
    namespace {
        bool           active_stack_guard_valid {};
        std::uintptr_t active_stack_guard_begin {};
        std::uintptr_t active_stack_guard_end {};
    }

    [[noreturn]] void dispatch(const info_t&) noexcept;

    void set_active_stack_guard(std::uintptr_t begin, std::uintptr_t end) noexcept
    {
        active_stack_guard_valid = false;
        active_stack_guard_begin = begin;
        active_stack_guard_end = end;
        active_stack_guard_valid = begin < end;
    }

    bool frame_readable(std::uint32_t sp, std::uint32_t cfsr, std::uint32_t exc_return)
    {
        constexpr auto stacking_errors = (std::uint32_t { 1 } << 4) | (std::uint32_t { 1 } << 5)
                                         | (std::uint32_t { 1 } << 12) | (std::uint32_t { 1 } << 13);

        const auto     extended_frame = (exc_return & 0x00000010U) == 0;
        const auto     frame_size = extended_frame ? sizeof(fpu_frame_t) : sizeof(core_frame_t);
        const auto     core_frame_offset = extended_frame ? offsetof(fpu_frame_t, core_frame) : 0;
        const auto     frame_begin = static_cast<std::uintptr_t>(sp);
        const auto     core_frame_begin = std::uint64_t { frame_begin } + core_frame_offset;
        const auto     core_frame_end = core_frame_begin + sizeof(core_frame_t);
        constexpr auto max_address = static_cast<std::uintptr_t>(-1);

        if (core_frame_end > max_address)
        {
            return false;
        }

        const auto overlaps_active_guard = active_stack_guard_valid && core_frame_begin < active_stack_guard_end
                                           && active_stack_guard_begin < core_frame_end;

        return (cfsr & stacking_errors) == 0 && (sp & 3U) == 0 && mcu::readable_ram(frame_begin, frame_size)
               && mcu::readable_ram(static_cast<std::uintptr_t>(core_frame_begin), sizeof(core_frame_t))
               && !overlaps_active_guard;
    }

    constexpr bool valid_exc_return(std::uint32_t value) noexcept
    {
        switch (value)
        {
            case 0xFFFFFFF1: // Handler, MSP, basic frame
            case 0xFFFFFFF9: // Thread,  MSP, basic frame
            case 0xFFFFFFFD: // Thread,  PSP, basic frame
            case 0xFFFFFFE1: // Handler, MSP, extended FP frame
            case 0xFFFFFFE9: // Thread,  MSP, extended FP frame
            case 0xFFFFFFED: // Thread,  PSP, extended FP frame
                return true;
            default:
                return false;
        }
    }
}

extern "C"
{
    volatile fault::record_t fault_record {};
}

extern "C" [[noreturn]] void fault_capture(std::uint32_t sp,
                                           std::uint32_t exc_return,
                                           std::uint32_t msp,
                                           std::uint32_t psp)
{
    using namespace artos::port::regmap;

    fault_record.complete = 0;

    std::uint32_t exception;
    asm volatile("mrs %0, ipsr" : "=r"(exception));

    fault_record.exception = exception;
    fault_record.exc_return = exc_return;
    fault_record.msp = msp;
    fault_record.psp = psp;

    const auto cfsr = SCB->CFSR.read();
    fault_record.cfsr = cfsr;
    fault_record.hfsr = SCB->HFSR.read();
    fault_record.dfsr = SCB->DFSR.read();
    fault_record.afsr = SCB->AFSR.read();
    fault_record.mmfar = SCB->MMFAR.read();
    fault_record.bfar = SCB->BFAR.read();
    fault_record.shcsr = SCB->SHCSR.read();

    const auto exc_return_valid = fault::valid_exc_return(exc_return);
    fault_record.frame_valid = exc_return_valid && fault::frame_readable(sp, cfsr, exc_return) ? 1U : 0U;

    std::uintptr_t interrupted_sp = 0;
    if (fault_record.frame_valid != 0)
    {
        const bool basic_frame = (exc_return & 0x00000010U) != 0;
        const auto frame_offset = basic_frame ? 0 : offsetof(fpu_frame_t, core_frame);
        const auto frame_bytes = basic_frame ? sizeof(core_frame_t) : sizeof(fpu_frame_t);

        const auto* frame
            = reinterpret_cast<volatile const fault::core_frame_t*>(static_cast<std::uintptr_t>(sp) + frame_offset);

        const auto padding_bytes = (frame->xpsr & (1U << 9)) ? sizeof(std::uint32_t) : 0;
        interrupted_sp = sp + frame_bytes + padding_bytes;

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

    constexpr auto entry_stacking_errors = scb::cfsr::mstkerr.mask | scb::cfsr::stkerr.mask;
    constexpr auto lazy_fp_errors = scb::cfsr::mlsperr.mask | scb::cfsr::lsperr.mask;
    constexpr auto unstacking_errors = scb::cfsr::munstkerr.mask | scb::cfsr::unstkerr.mask;

    const bool lazy_fp_fault = (cfsr & lazy_fp_errors) != 0;
    const bool entry_stacking_fault = (cfsr & entry_stacking_errors) != 0;
    const bool unstacking_fault = (cfsr & unstacking_errors) != 0;

    info_t fault_info {};

    if (entry_stacking_fault && !unstacking_fault)
    {
        fault_info.cause = cause_t::exception_stack_access;
    }
    else if (unstacking_fault && !entry_stacking_fault)
    {
        fault_info.cause = cause_t::exception_unstack_access;
    }
    else if (entry_stacking_fault || unstacking_fault)
    {
        // Conflicting status flags: don't guess which operation failed.
        fault_info.cause = cause_t::other;
    }
    else if (lazy_fp_fault)
    {
        fault_info.cause = cause_t::exception_lazy_stack_access;
    }
    else if (cfsr & (scb::cfsr::daccviol.mask | scb::cfsr::iaccviol.mask))
    {
        fault_info.cause = cause_t::memory_access;
    }
    else
    {
        fault_info.cause = cause_t::other;
    }

    if (exc_return_valid)
    {
        if (fault_record.exc_return & 0x0000000C)
        {
            fault_info.origin = origin_t::thread;
        }
        else if ((fault_record.exc_return & 0x00000008) == 0)
        {
            fault_info.origin = origin_t::handler;
        }
    }
    else
    {
        fault_info.origin = origin_t::unknown;
    }

    if ((cfsr & scb::cfsr::mmarvalid.mask) != 0)
    {
        fault_info.address = SCB->MMFAR.read();
    }

    if (exc_return_valid && fault_record.frame_valid && fault_info.origin == origin_t::thread)
    {
        fault_info.interrupted_stack_pointer = interrupted_sp;
    }

    fault_info.exception_frame_valid = fault_record.frame_valid;
    fault_info.exception_frame_address = sp;

    if (exc_return_valid && entry_stacking_fault && !unstacking_fault && !lazy_fp_fault)
    {
        const bool basic_frame = (exc_return & 0x00000010U) != 0;
        const auto frame_bytes = basic_frame ? sizeof(core_frame_t) : sizeof(fpu_frame_t);
        const auto end = static_cast<std::uint64_t>(sp) + frame_bytes;

        if (end <= UINTPTR_MAX)
        {
            fault_info.exception_stack_access_range = { sp, static_cast<std::uintptr_t>(end) };
        }
    }

    if (exc_return_valid)
    {
        fault_info.stack_type = (exc_return & 0x00000004) ? stack_type_t::process : stack_type_t::main;
    }

    fault::dispatch(fault_info);
}
