#include <build_config.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/thread_queue.h>

#include <cassert>

namespace artos {
    bool thread_queue_t::empty() const noexcept
    {
        return list.empty();
    }

    void thread_queue_t::push_back(thread_queue_node_t& node) noexcept
    {
        assert(node.queue == nullptr);

        list.push_back(node.hook);
        node.queue = this;
    }

    void thread_queue_t::remove(thread_queue_node_t& node) noexcept
    {
        assert(node.queue == this);

        list.erase(node.hook);
        node.queue = nullptr;
    }

    void thread_queue_t::clear() noexcept
    {
        auto* p = list.front();
        while (p != list.end())
        {
            auto* next = p->next;
            list.erase(*p);
            auto node = reinterpret_cast<thread_queue_node_t*>(p);
            node->queue = nullptr;
            p = next;
        }
    }

    void thread_queue_t::rotate_after(thread_queue_node_t& node) noexcept
    {
        list.rotate_after(node.hook);
    }

    thread_queue_node_t* thread_queue_t::front() noexcept
    {
        return reinterpret_cast<thread_queue_node_t*>(list.front());
    }

    void thread_queue_t::pop_front() noexcept
    {
        thread_queue_node_t* node = reinterpret_cast<thread_queue_node_t*>(list.front());
        if (node)
        {
            remove(*node);
        }
    }

    thread_queue_iterator_t thread_queue_t::begin() noexcept
    {
        return thread_queue_iterator_t { reinterpret_cast<thread_queue_node_t*>(list.front()) };
    }

    thread_queue_iterator_t thread_queue_t::end() noexcept
    {
        return thread_queue_iterator_t { reinterpret_cast<thread_queue_node_t*>(list.end()) };
    }

    void thread_queue_t::insert(thread_queue_iterator_t location, thread_queue_node_t& node) noexcept
    {
        list.insert(location->hook, node.hook);
        node.queue = this;
    }
}