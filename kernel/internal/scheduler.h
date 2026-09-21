#pragma once

#include <build_config.h>

#include <array>
#include <cstddef>

#include <intrusive_list.h>
#include <wait.h>

// Private declarations for the existing shared state. Defined in scheduler.cpp.
namespace artos::thread::internal {
    struct tcb_base_t;

    class ready_queue_t;

    struct ready_node_t
    {
        list_hook_t                   hook;
        thread::internal::tcb_base_t* thread {};
        ready_queue_t*                queue { nullptr };
    };

    class ready_queue_iterator_t
    {
    public:
        ready_queue_iterator_t(ready_node_t* ready_node) noexcept
            : node { ready_node }
        {
        }

        ~ready_queue_iterator_t() noexcept = default;

        ready_node_t& operator*() noexcept
        {
            return *node;
        }

        ready_node_t* operator->() noexcept
        {
            return node;
        }

        bool operator==(const ready_queue_iterator_t& other) const noexcept = default;

        ready_queue_iterator_t operator++() noexcept
        {
            node = reinterpret_cast<ready_node_t*>(node->hook.next);
            return *this;
        }

    private:
        ready_node_t* node {};
    };

    class ready_queue_t
    {
    public:
        bool empty() const noexcept;
        void push_back(ready_node_t& node) noexcept;
        void push_front(ready_node_t& node) noexcept;
        void remove(ready_node_t& node) noexcept;
        void clear() noexcept;
        void rotate_after(ready_node_t& node) noexcept;

        ready_node_t* front() noexcept;

    private:
        intrusive_list_t waiters {};
    };

    extern std::array<ready_queue_t, build_config::artos::priority_levels> ready_queue;
    extern std::array<tcb_base_t*, build_config::artos::max_threads>       threads;
    extern std::array<std::uint32_t, 8>                                    ready_bitmap;

    extern std::size_t thread_count;
    extern tcb_base_t* current_thread;
    extern tcb_base_t* next_thread;
}

namespace artos::kernel::internal {

    thread::internal::tcb_base_t* select_next() noexcept;

    void sched() noexcept;
}
