#pragma once
#include <deferred.h>

#include <atomic>

namespace artos::deferred::internal {
    struct deferred_slot_t
    {
        deferred_callback_t callback {};
        void*               context {};
        std::atomic<bool>   pending { false };
        static_assert(std::atomic<bool>::is_always_lock_free);
    };

    void serve_deferred() noexcept;
}