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

    class wait_queue_iterator_t
    {
    public:
        wait_queue_iterator_t(wait_node_t* wait_node) noexcept
            : node { wait_node }
        {
        }

        ~wait_queue_iterator_t() noexcept = default;

        wait_node_t& operator*() noexcept
        {
            return *node;
        }

        wait_node_t* operator->() noexcept
        {
            return node;
        }

        bool operator==(const wait_queue_iterator_t& other) const noexcept = default;

        wait_queue_iterator_t operator++() noexcept
        {
            node = reinterpret_cast<wait_node_t*>(node->hook.next);
            return *this;
        }

    private:
        wait_node_t* node {};
    };

    class wait_queue_t
    {
    public:
        bool empty() const noexcept;
        void push_back(wait_node_t& node) noexcept;
        void remove(wait_node_t& node) noexcept;

        wait_queue_iterator_t begin() noexcept;
        wait_queue_iterator_t end() noexcept;

    private:
        intrusive_list_t waiters {};
    };
}