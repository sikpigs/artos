#pragma once

#include <intrusive_list.h>

namespace artos {
    namespace thread::internal {
        struct tcb_base_t;
    }

    class thread_queue_t;

    struct thread_queue_node_t
    {
        explicit thread_queue_node_t(thread::internal::tcb_base_t* thread_in) noexcept
            : thread { thread_in }
        {
        }

        list_hook_t                   hook {};
        thread::internal::tcb_base_t* thread;
        thread_queue_t*               queue { nullptr };
    };

    class thread_queue_iterator_t
    {
    public:
        thread_queue_iterator_t(thread_queue_node_t* wait_node) noexcept
            : node { wait_node }
        {
        }

        ~thread_queue_iterator_t() noexcept = default;

        thread_queue_node_t& operator*() noexcept
        {
            return *node;
        }

        thread_queue_node_t* operator->() noexcept
        {
            return node;
        }

        bool operator==(const thread_queue_iterator_t& other) const noexcept = default;

        thread_queue_iterator_t operator++() noexcept
        {
            node = reinterpret_cast<thread_queue_node_t*>(node->hook.next);
            return *this;
        }

    private:
        thread_queue_node_t* node {};
    };

    class thread_queue_t
    {
    public:
        bool empty() const noexcept;
        void push_back(thread_queue_node_t& node) noexcept;
        void remove(thread_queue_node_t& node) noexcept;
        void clear() noexcept;
        void rotate_after(thread_queue_node_t& node) noexcept;

        void                 pop_front() noexcept;
        thread_queue_node_t* front() noexcept;

        thread_queue_iterator_t begin() noexcept;
        thread_queue_iterator_t end() noexcept;

        void insert(thread_queue_iterator_t location, thread_queue_node_t& node) noexcept;

    private:
        intrusive_list_t list {};
    };
}