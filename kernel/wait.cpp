#include <cassert>

#include <utility>
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

    const wait_node_t* wait_queue_t::front() const noexcept
    {
        return reinterpret_cast<const wait_node_t*>(waiters.front());
    }

    wait_node_t* wait_queue_t::front() noexcept
    {
        return const_cast<wait_node_t*>(std::as_const(*this).front());
    }
}