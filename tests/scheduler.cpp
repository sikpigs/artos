#include <build_config.h>

#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/thread_queue.h>
#include <internal/thread_state.h>

#include <artos.h>
#include <artos_port.h>
#include <wait_queue.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sys/wait.h>
#include <unistd.h>

namespace {
    using artos::thread::internal::tcb_base_t;
    using artos::thread::internal::thread_state_t;

    artos::tick_time_t                                       now {};
    unsigned                                                 pendsv_count = 0;
    std::array<tcb_base_t, build_config::artos::max_threads> tcbs {};

    void check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::printf("FAIL: %s\n", message);
            std::exit(1);
        }
    }

    artos::thread::priority_t priority(std::uint8_t value)
    {
        return static_cast<artos::thread::priority_t>(value);
    }

    bool is_runnable(thread_state_t state)
    {
        return state == thread_state_t::ready || state == thread_state_t::running;
    }

    void enqueue_ready(tcb_base_t& thread)
    {
        using namespace artos::thread::internal;

        check(thread.ready.queue == nullptr, "thread was already in a ready queue");
        ready_queue[std::to_underlying(thread.priority)].push_back(thread.ready);
        set_priority_ready(thread.priority);
    }

    void remove_ready(tcb_base_t& thread)
    {
        using namespace artos::thread::internal;

        if (thread.ready.queue == nullptr)
        {
            return;
        }

        auto* queue = thread.ready.queue;
        queue->remove(thread.ready);
        if (queue->empty())
        {
            clear_priority_ready(thread.priority);
        }
    }

    tcb_base_t& install(std::size_t slot, std::uint8_t priority_value, thread_state_t state)
    {
        using namespace artos::thread::internal;

        auto& thread = tcbs[slot];
        check(threads[slot] == nullptr, "test installed the same thread slot twice");
        check(thread.ready.queue == nullptr, "test installed a thread that was already ready-queued");
        check(thread.waiter.queue == nullptr, "test installed a thread that was still waiting");

        thread.priority = priority(priority_value);
        thread.state = state;
        thread.deadline = {};
        thread.ready.thread = &thread;
        thread.waiter.thread = &thread;
        thread.waiter.wake = artos::wake_reason_t::none;
        threads[slot] = &thread;
        ++thread_count;

        if (is_runnable(state))
        {
            enqueue_ready(thread);
        }

        return thread;
    }

    void set_current(tcb_base_t& thread)
    {
        using namespace artos::thread::internal;

        check(thread.ready.queue != nullptr, "current thread was not ready-queued");
        if (current_thread != nullptr && current_thread->state == thread_state_t::running)
        {
            current_thread->state = thread_state_t::ready;
        }
        thread.state = thread_state_t::running;
        current_thread = &thread;
    }

    void make_not_ready(tcb_base_t& thread, thread_state_t state)
    {
        check(!is_runnable(state), "non-ready transition used a runnable state");
        remove_ready(thread);
        thread.state = state;
    }

    void reset_scheduler()
    {
        using namespace artos::thread::internal;

        for (auto& queue : ready_queue)
        {
            queue.clear();
        }
        block_sleep_queue.clear();
        ready_bitmap.fill(0);

        for (auto& thread : tcbs)
        {
            check(thread.waiter.queue == nullptr, "previous test left a waiter queued");
            thread.state = thread_state_t::inactive;
            thread.deadline = {};
            thread.waiter.wake = artos::wake_reason_t::none;
        }

        threads.fill(nullptr);
        thread_count = 0;
        current_thread = nullptr;
        next_thread = nullptr;
        now = {};
        pendsv_count = 0;
        (void)install(0, 15, thread_state_t::suspended);
    }

    void test_highest_priority_ready_thread_wins()
    {
        reset_scheduler();
        auto& current = install(1, 8, thread_state_t::running);
        (void)install(2, 5, thread_state_t::ready);
        auto& highest = install(5, 2, thread_state_t::ready);
        set_current(current);

        check(artos::kernel::internal::select_next() == &highest,
              "selector did not choose the highest-priority ready thread");
    }

    void test_equal_priority_threads_rotate()
    {
        reset_scheduler();
        auto& current = install(1, 4, thread_state_t::running);
        auto& first = install(2, 4, thread_state_t::ready);
        auto& second = install(3, 4, thread_state_t::ready);
        set_current(current);

        check(artos::kernel::internal::select_next() == &first,
              "round-robin selection did not advance to the first peer");
        set_current(first);
        check(artos::kernel::internal::select_next() == &second,
              "round-robin selection did not advance to the second peer");
    }

    void test_non_runnable_threads_are_not_ready_queued()
    {
        reset_scheduler();
        auto& current = install(1, 7, thread_state_t::running);
        auto& blocked = install(2, 0, thread_state_t::blocked_forever);
        auto& suspended = install(3, 1, thread_state_t::suspended);
        auto& finished = install(4, 2, thread_state_t::finished);
        set_current(current);

        check(blocked.ready.queue == nullptr, "blocked thread was ready-queued");
        check(suspended.ready.queue == nullptr, "suspended thread was ready-queued");
        check(finished.ready.queue == nullptr, "finished thread was ready-queued");
        check(artos::kernel::internal::select_next() == &current, "selector did not retain the only runnable thread");
    }

    void test_idle_is_only_the_fallback()
    {
        reset_scheduler();
        auto& idle = tcbs[0];
        auto& current = install(1, 8, thread_state_t::running);
        set_current(current);
        make_not_ready(current, thread_state_t::blocked_forever);

        check(artos::kernel::internal::select_next() == &idle,
              "selector did not fall back to idle when all applications blocked");

        auto& application = install(2, 15, thread_state_t::ready);
        check(artos::kernel::internal::select_next() == &application,
              "idle competed with a runnable lowest-priority application thread");
    }

    void test_sleep_deadlines_control_eligibility()
    {
        reset_scheduler();
        now = { .ticks = 100, .generation = 3 };
        auto& current = install(1, 8, thread_state_t::running);
        auto& due = install(2, 2, thread_state_t::sleeping);
        due.deadline = { .ticks = 100, .generation = 3 };
        auto& future = install(3, 0, thread_state_t::sleeping);
        future.deadline = { .ticks = 101, .generation = 3 };
        artos::thread::internal::block_sleep_thread(&future);
        artos::thread::internal::block_sleep_thread(&due);
        set_current(current);

        artos::kernel::internal::sched();

        check(artos::thread::internal::next_thread == &due, "scheduler did not select the due sleeper");
        check(due.state == thread_state_t::ready, "due sleeper was not transitioned to ready");
        check(due.ready.queue != nullptr, "due sleeper was not inserted into a ready queue");
        check(due.block_sleep.queue == nullptr, "due sleeper remained in the deadline queue");
        check(future.state == thread_state_t::sleeping, "future sleeper was readied early");
        check(future.ready.queue == nullptr, "future sleeper was inserted into a ready queue");
        check(future.block_sleep.queue != nullptr, "future sleeper was removed from the deadline queue");
    }

    void test_block_timeout_removes_waiter()
    {
        reset_scheduler();
        now = { .ticks = 50, .generation = 1 };
        auto& current = install(1, 9, thread_state_t::running);
        auto& timed = install(2, 3, thread_state_t::blocked);
        timed.deadline = now;
        artos::wait_queue_t queue;
        queue.push_back(timed.waiter);
        artos::thread::internal::block_sleep_thread(&timed);
        set_current(current);

        artos::kernel::internal::sched();

        check(artos::thread::internal::next_thread == &timed, "scheduler did not select the timed-out thread");
        check(timed.state == thread_state_t::ready, "timed-out thread was not made ready");
        check(timed.ready.queue != nullptr, "timed-out thread was not inserted into a ready queue");
        check(timed.block_sleep.queue == nullptr, "timed-out thread remained in the deadline queue");
        check(timed.waiter.wake == artos::wake_reason_t::timeout, "timed-out waiter recorded the wrong wake reason");
        check(timed.waiter.queue == nullptr && queue.empty(), "timed-out waiter remained in its wait queue");
    }

    void test_deadline_generation_wrap()
    {
        reset_scheduler();
        now = { .ticks = 1, .generation = 4 };
        auto& current = install(1, 8, thread_state_t::running);
        auto& wrapped = install(2, 2, thread_state_t::sleeping);
        wrapped.deadline = { .ticks = std::numeric_limits<artos::tick_t>::max(), .generation = 3 };
        auto& next_generation = install(3, 0, thread_state_t::sleeping);
        next_generation.deadline = { .ticks = 1, .generation = 5 };
        artos::thread::internal::block_sleep_thread(&next_generation);
        artos::thread::internal::block_sleep_thread(&wrapped);
        set_current(current);

        artos::kernel::internal::sched();

        check(artos::thread::internal::next_thread == &wrapped,
              "scheduler did not select the sleeper spanning tick rollover");
        check(wrapped.state == thread_state_t::ready,
              "deadline immediately before tick rollover was not recognized as elapsed");
        check(next_generation.state == thread_state_t::sleeping,
              "deadline in the next generation was treated as elapsed");
    }

    void test_sched_refreshes_pending_selection()
    {
        reset_scheduler();
        auto& current = install(1, 8, thread_state_t::running);
        auto& higher = install(2, 2, thread_state_t::ready);
        set_current(current);

        artos::kernel::internal::sched();
        check(artos::thread::internal::next_thread == &higher, "sched did not publish its selection");
        check(pendsv_count == 1, "sched did not pend a required context switch");

        make_not_ready(higher, thread_state_t::blocked_forever);
        artos::kernel::internal::sched();
        check(artos::thread::internal::next_thread == &current, "sched did not refresh a stale pending destination");
        check(pendsv_count == 1, "sched pended a switch when the current thread remained selected");
    }

    void test_sched_preserves_startup_selection()
    {
        reset_scheduler();
        auto& selected = install(1, 2, thread_state_t::ready);
        artos::thread::internal::next_thread = &selected;

        artos::kernel::internal::sched();
        check(artos::thread::internal::next_thread == &selected,
              "sched overwrote the startup selection before a context existed");
        check(pendsv_count == 0, "sched pended PendSV before the first context started");
    }

    void test_initial_selection_uses_the_highest_priority_thread()
    {
        reset_scheduler();
        (void)install(1, 5, thread_state_t::ready);
        auto& highest = install(2, 2, thread_state_t::ready);

        check(artos::kernel::internal::select_next() == &highest,
              "initial selection did not choose the highest-priority ready thread");
    }

    extern "C" bool                         artos_context_started() noexcept;
    extern "C" artos::thread::stack_word_t* artos_context_start() noexcept;

    void test_context_start_and_switch_state()
    {
        reset_scheduler();
        artos::thread::stack_word_t first_stack {};
        artos::thread::stack_word_t second_stack {};
        artos::thread::stack_word_t saved_stack {};
        auto&                       first = install(1, 4, thread_state_t::ready);
        auto&                       second = install(2, 4, thread_state_t::ready);
        first.sp = &first_stack;
        second.sp = &second_stack;
        artos::thread::internal::next_thread = &first;

        check(!artos_context_started(), "context reported started before first dispatch");
        check(artos_context_start() == &first_stack, "context start returned the wrong stack");
        check(artos_context_started(), "context did not report started after first dispatch");
        check(first.state == thread_state_t::running, "first context was not marked running");

        artos::thread::internal::next_thread = &second;
        check(artos::thread::internal::artos_context_switch(&saved_stack) == &second_stack,
              "context switch returned the wrong incoming stack");
        check(first.sp == &saved_stack && first.state == thread_state_t::ready,
              "outgoing context was not saved and made ready");
        check(second.state == thread_state_t::running, "incoming context was not marked running");
    }

    struct test_case_t
    {
        const char* name;
        void (*run)();
    };

    const std::array cases {
        test_case_t { "highest priority", test_highest_priority_ready_thread_wins },
        test_case_t { "equal-priority rotation", test_equal_priority_threads_rotate },
        test_case_t { "ready-queue membership", test_non_runnable_threads_are_not_ready_queued },
        test_case_t { "idle fallback", test_idle_is_only_the_fallback },
        test_case_t { "sleep deadlines", test_sleep_deadlines_control_eligibility },
        test_case_t { "blocked timeout", test_block_timeout_removes_waiter },
        test_case_t { "deadline generation wrap", test_deadline_generation_wrap },
        test_case_t { "pending selection", test_sched_refreshes_pending_selection },
        test_case_t { "startup selection", test_sched_preserves_startup_selection },
        test_case_t { "initial selection", test_initial_selection_uses_the_highest_priority_thread },
        test_case_t { "context state transitions", test_context_start_and_switch_state },
    };
}

namespace artos::kernel {
    tick_time_t tick_time() noexcept
    {
        return now;
    }
}

namespace artos::port {
    void pendsvset() noexcept
    {
        ++pendsv_count;
    }
}

int main()
{
    std::size_t failures = 0;

    for (const auto& test : cases)
    {
        const auto child = fork();
        check(child >= 0, "fork failed");

        if (child == 0)
        {
            alarm(2);
            test.run();
            _Exit(0);
        }

        int status = 0;
        check(waitpid(child, &status, 0) == child, "waitpid failed");
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            std::printf("PASS: %s\n", test.name);
        }
        else
        {
            std::printf("FAIL: %s\n", test.name);
            ++failures;
        }
    }

    std::printf("%zu tests, %zu failed\n", cases.size(), failures);
    return failures == 0 ? 0 : 1;
}
