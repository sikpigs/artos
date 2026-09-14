#pragma once

#include <cstdint>

#include <intrusive_list.h>

namespace artos {
    namespace thread::internal {
        struct tcb_t;
    }

    enum class wake_reason_t : std::uint8_t
    {
        none,
        signaled,
        timeout,
        cancelled
    };

    class wait_queue_t;

    struct wait_node_t
    {
        list_hook_t              hook;
        thread::internal::tcb_t* thread {};
        wake_reason_t            wake { wake_reason_t::none };
        wait_queue_t*            queue { nullptr };
    };

    class wait_queue_t
    {
    public:
        bool empty() const noexcept;
        void push_back(wait_node_t& node) noexcept;
        void remove(wait_node_t& node) noexcept;

        wait_node_t*       front() noexcept;
        const wait_node_t* front() const noexcept;

    private:
        intrusive_list_t waiters {};
    };
}