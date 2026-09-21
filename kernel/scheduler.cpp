#include <build_config.h>

#include "internal/scheduler.h"
#include "internal/thread.h"
#include "internal/time.h"

#include <cassert>

#include <artos.h>
#include <artos_port.h>
#include <utility>
#include <wait.h>

using namespace build_config::artos;

namespace artos::thread::internal {
    std::array<ready_queue_t, priority_levels> ready_queue {};
    std::array<tcb_base_t*, max_threads>       threads {};
    std::array<std::uint32_t, 8>               ready_bitmap {};

    std::size_t thread_count = 0;
    tcb_base_t* current_thread = nullptr;
    tcb_base_t* next_thread = nullptr;

    extern "C" bool artos_context_started() noexcept
    {
        return current_thread != nullptr;
    }

    extern "C" stack_word_t* artos_context_start() noexcept
    {
        current_thread = next_thread;
        next_thread = nullptr;
        current_thread->state = thread_state_t::running;
        return current_thread->sp;
    }

    extern "C" stack_word_t* artos_context_switch(stack_word_t* outgoing_sp) noexcept
    {
        if (current_thread != nullptr)
        {
            current_thread->sp = outgoing_sp;
            if (current_thread->state == thread_state_t::running)
            {
                current_thread->state = thread_state_t::ready;
            }
        }

        current_thread = next_thread;
        current_thread->state = thread_state_t::running;
        next_thread = nullptr;

        return current_thread->sp;
    }

    bool ready_queue_t::empty() const noexcept
    {
        return waiters.empty();
    }

    void ready_queue_t::push_back(ready_node_t& node) noexcept
    {
        assert(node.queue == nullptr);

        waiters.push_back(node.hook);
        node.queue = this;
    }

    void ready_queue_t::push_front(ready_node_t& node) noexcept
    {
        assert(node.queue == nullptr);

        waiters.push_front(node.hook);
        node.queue = this;
    }

    void ready_queue_t::remove(ready_node_t& node) noexcept
    {
        assert(node.queue == this);

        waiters.erase(node.hook);
        node.queue = nullptr;
    }

    void ready_queue_t::rotate_after(ready_node_t& node) noexcept
    {
        waiters.erase(node.hook);
        waiters.push_back(node.hook);
    }

    void ready_queue_t::clear() noexcept
    {
        auto* p = waiters.front();
        while (p != waiters.end())
        {
            auto* next = p->next;
            waiters.erase(*p);
            auto node = reinterpret_cast<ready_node_t*>(p);
            node->queue = nullptr;
            p = next;
        }
    }

    ready_node_t* ready_queue_t::front() noexcept
    {
        return reinterpret_cast<ready_node_t*>(waiters.front());
    }

    void set_priority_ready(priority_t priority) noexcept
    {
        auto p = std::to_underlying(priority);
        ready_bitmap[p >> 5U] |= 1U << (p & 31U);
    }

    void clear_priority_ready(priority_t priority) noexcept
    {
        auto p = std::to_underlying(priority);
        ready_bitmap[p >> 5U] &= ~(1U << (p & 31U));
    }

    std::optional<priority_t> get_highest_set_priority() noexcept
    {
        auto priority = std::to_underlying(priority_t::high);

        for (std::size_t ix = 0; ix < ready_bitmap.size(); ++ix)
        {
            auto& bitmap = ready_bitmap[ix];
            auto  p = static_cast<std::uint32_t>(std::countr_zero(bitmap));
            priority += p;
            if (p < std::numeric_limits<std::uint32_t>::digits)
            {
                return static_cast<priority_t>(priority);
            }
        }

        return std::nullopt;
    }
}

namespace artos::kernel::internal {
    using namespace thread::internal;

    using tcb_t = tcb_t<decltype(scheduler)>;

    void check_deadlines() noexcept
    {
        const auto now = kernel::tick_time();

        for (std::size_t slot = 1; slot < threads.size(); ++slot)
        {
            auto* thread = threads[slot];

            if (thread == nullptr)
            {
                continue;
            }

            if ((thread->state == thread_state_t::sleeping || thread->state == thread_state_t::blocked)
                && deadline_reached(now, thread->deadline))
            {
                if (thread->state == thread_state_t::blocked)
                {
                    thread->waiter.queue->remove(thread->waiter);
                    thread->waiter.wake = wake_reason_t::timeout;
                }

                thread->state = thread_state_t::ready;
                ready_queue[std::to_underlying(thread->priority)].push_back(thread->ready);
                set_priority_ready(thread->priority);
            }
        }
    }

    [[nodiscard]]
    thread::internal::tcb_base_t* select_next(fixed_priority_round_robin_config) noexcept
    {
        auto priority = get_highest_set_priority();
        if (!priority)
        {
            return threads[0];
        }

        auto& queue = ready_queue[std::to_underlying(*priority)];

        if (current_thread != nullptr && current_thread != threads[0]
            && current_thread->state == thread_state_t::running)
        {
            current_thread->ready.queue->rotate_after(current_thread->ready);
        }

        return queue.front()->thread;
    }

    thread::internal::tcb_base_t* select_next() noexcept
    {
        return select_next(build_config::artos::scheduler);
    }

    void sched() noexcept
    {
        // Leave the PendSV startup selection intact until a thread is running.
        if (thread::internal::current_thread == nullptr)
        {
            return;
        }

        check_deadlines();

        auto* selected = select_next(scheduler);
        // Refresh even if a previously requested PendSV is already pending
        // and a new tick now selects the current thread again.
        thread::internal::next_thread = selected;
        if (selected != thread::internal::current_thread)
        {
            artos::port::pendsvset();
        }
    }
}
