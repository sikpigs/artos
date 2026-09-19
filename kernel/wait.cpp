#include <cassert>

#include <wait.h>

namespace artos {
    bool wait_queue_t::empty() const noexcept
    {
        return waiters.empty();
    }

    void wait_queue_t::push_back(wait_node_t& node) noexcept
    {
        assert(node.queue == nullptr);

        waiters.push_back(node.hook);
        node.queue = this;
    }

    void wait_queue_t::remove(wait_node_t& node) noexcept
    {
        assert(node.queue == this);

        waiters.erase(node.hook);
        node.queue = nullptr;
    }

    wait_queue_iterator_t wait_queue_t::begin() noexcept
    {
        return wait_queue_iterator_t { reinterpret_cast<wait_node_t*>(waiters.front()) };
    }

    wait_queue_iterator_t wait_queue_t::end() noexcept
    {
        return wait_queue_iterator_t { reinterpret_cast<wait_node_t*>(waiters.end()) };
    }
}