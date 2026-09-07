extern "C" void _init() __attribute__((weak));

extern "C" void __libc_init_array()
{
    using init_function = void (*)();

    extern init_function __preinit_array_start[];
    extern init_function __preinit_array_end[];

    extern init_function __init_array_start[];
    extern init_function __init_array_end[];

    for (auto p = __preinit_array_start; p != __preinit_array_end; ++p)
    {
        (*p)();
    }

    if (_init != nullptr)
    {
        _init();
    }

    for (auto p = __init_array_start; p != __init_array_end; ++p)
    {
        (*p)();
    }
}