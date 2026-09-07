extern "C" [[noreturn]] void abort() noexcept
{
    __asm volatile("bkpt #0");
    for (;;)
    {
    }
}