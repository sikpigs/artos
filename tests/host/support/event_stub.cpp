#include <event.h>

namespace artos {
    bool event_t::satisfied(any_or_all_t) noexcept
    {
        return false;
    }

    void event_t::clear(any_or_all_t) noexcept
    {
    }
}
