#include <build_config.h>

#include <array>
#include <atomic>
#include <cassert>

#include <deferred.h>

#include "internal/deferred.h"

namespace artos::deferred {
    namespace internal {
        std::array<deferred_slot_t, build_config::artos::max_deferred_work_items> deferred_slots {};

        void serve_deferred() noexcept
        {
            for (auto& slot : internal::deferred_slots)
            {
                if (slot.callback && slot.pending.exchange(false, std::memory_order_acquire))
                {
                    slot.callback(slot.context);
                }
            }
        }
    }

    handle_t register_deferred(deferred_callback_t callback, void* context) noexcept
    {
        if (callback)
        {
            for (std::size_t ix = 0; ix < internal::deferred_slots.size(); ++ix)
            {
                if (internal::deferred_slots[ix].callback == nullptr)
                {
                    internal::deferred_slots[ix].callback = callback;
                    internal::deferred_slots[ix].context = context;
                    internal::deferred_slots[ix].pending.store(false, std::memory_order_release);
                    return { .index = ix };
                }
            }
        }

        return invalid_deferred_handle;
    }

    void pend_deferred(const handle_t& handle) noexcept
    {
        assert(handle.index < internal::deferred_slots.size());
        assert(internal::deferred_slots[handle.index].callback);
        internal::deferred_slots[handle.index].pending.store(true, std::memory_order_release);
    }
}