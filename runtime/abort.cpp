#include <cpu_port.h>

extern "C" [[noreturn]] void abort() noexcept
{
    cpu::port::fatal_trap();
}
