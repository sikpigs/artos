#include <arch/fault_capture.h>
#include <cstddef>

// Cortex-M4 basic and FP-extended frames both place PC at SP + 24.
static_assert(sizeof(fault::core_frame_t) == 32);
static_assert(offsetof(fault::core_frame_t, pc) == 24);
