#include <cassert>

#include <wait_queue.h>

namespace artos {
    bool wait_queue_t::empty() const noexcept
    {
        return waiters.empty();
    }

    void wait_queue_t::push_back(wait_queue_node_t& node) noexcept
    {
        assert(node.queue == nullptr);

        waiters.push_back(node.hook);
        node.queue = this;
    }

    void wait_queue_t::push_front(wait_queue_node_t& node) noexcept
    {
        assert(node.queue == nullptr);

        waiters.push_front(node.hook);
        node.queue = this;
    }

    void wait_queue_t::remove(wait_queue_node_t& node) noexcept
    {
        assert(node.queue == this);

        waiters.erase(node.hook);
        node.queue = nullptr;
    }

    wait_queue_iterator_t wait_queue_t::begin() noexcept
    {
        return wait_queue_iterator_t { reinterpret_cast<wait_queue_node_t*>(waiters.front()) };
    }

    wait_queue_iterator_t wait_queue_t::end() noexcept
    {
        return wait_queue_iterator_t { reinterpret_cast<wait_queue_node_t*>(waiters.end()) };
    }

    wait_queue_iterator_t wait_queue_t::after(const wait_queue_node_t& node) noexcept
    {
        return wait_queue_iterator_t { reinterpret_cast<wait_queue_node_t*>(node.hook.next) };
    }

    void wait_queue_t::clear() noexcept
    {
        auto* p = waiters.front();
        while (p != waiters.end())
        {
            auto* next = p->next;
            waiters.erase(*p);
            auto node = reinterpret_cast<wait_queue_node_t*>(p);
            node->queue = nullptr;
            p = next;
        }
    }
}