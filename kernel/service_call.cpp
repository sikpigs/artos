#include <cstdint>

extern "C"
{
    void service_call(std::uint32_t svc_call, std::uint32_t* args)
    {
        switch (svc_call)
        {
            case 0:
                args[0] = 42;
                break;
        }
    }
}