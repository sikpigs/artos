#include <cstddef>
#include <fault_capture.h>

// Cortex-M4 basic and FP-extended frames both place PC at SP + 24.
static_assert(sizeof(fault::core_frame) == 32);
static_assert(offsetof(fault::core_frame, pc) == 24);
static_assert(fault::frame_readable(0x20000000U, 0));
static_assert(fault::frame_readable(mcu::ram_start + mcu::ram_size - 32, 0));
static_assert(!fault::frame_readable(mcu::ram_start + mcu::ram_size - 28, 0));
static_assert(!fault::frame_readable(0x20000001U, 0));
static_assert(!fault::frame_readable(0x1FFFFFFCU, 0));
static_assert(!fault::frame_readable(0xFFFFFFFCU, 0));
static_assert(!fault::frame_readable(0, 0));
// Do not dereference frames when stacking or lazy FP preservation failed.
static_assert(!fault::frame_readable(0x20001000U, 1U << 4));
static_assert(!fault::frame_readable(0x20001000U, 1U << 5));
static_assert(!fault::frame_readable(0x20001000U, 1U << 12));
static_assert(!fault::frame_readable(0x20001000U, 1U << 13));
// An undefined instruction or precise bus fault still has useful core registers.
static_assert(fault::frame_readable(0x20001000U, 1U << 16));
static_assert(fault::frame_readable(0x20001000U, 1U << 9));
