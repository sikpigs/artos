#include <board_services.h>
#include <build_config.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>
#include <internal/thread_queue.h>

#include <artos.h>
#include <artos_port.h>
#include <wait_queue.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <sys/wait.h>
#include <unistd.h>

namespace {
    using artos::kernel::internal::threads;
    using artos::kernel::internal::next_thread;
    using artos::kernel::internal::current_thread;
    using artos::kernel::internal::ready_queue;
    using artos::thread::internal::tcb_t;
    using artos::thread::state_t;

    artos::tick_time_t                                                        now {};
    unsigned                                                                  pendsv_count = 0;
    std::uint32_t                                                             cycles_remaining = 16000;
    std::array<std::optional<tcb_t>, build_config::artos::max_threads>        tcbs {};
    std::array<artos::thread::stack_word_t, build_config::artos::max_threads> stacks {};

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

    bool is_runnable(state_t state)
    {
        return state == state_t::ready || state == state_t::running;
    }

    void enqueue_ready(tcb_t& thread)
    {
        using namespace artos::thread::internal;

        check(thread.ready.queue == nullptr, "thread was already in a ready queue");
        artos::kernel::internal::ready_queue[std::to_underlying(thread.priority)].push_back(thread.ready);
        set_priority_ready(thread.priority);
    }

    void remove_ready(tcb_t& thread)
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

    tcb_t& install(std::size_t slot, std::uint8_t priority_value, state_t state, bool suspended = false)
    {
        using namespace artos::thread::internal;

        check(artos::kernel::internal::threads[slot] == nullptr, "test installed the same thread slot twice");
        check(!tcbs[slot].has_value(), "test installed the same thread slot twice");
        auto& thread = tcbs[slot].emplace("test", priority(priority_value), &stacks[slot], &stacks[slot], 1);
        (void)thread.make_thread_id(slot);
        thread.state = state;
        thread.suspended = suspended;
        thread.deadline = {};
        thread.waiter.wake = artos::wake_reason_t::none;
        artos::kernel::internal::threads[slot] = &thread;
        ++artos::kernel::internal::thread_count;

        if (slot != 0 && is_runnable(state) && !suspended)
        {
            enqueue_ready(thread);
        }

        return thread;
    }

    void set_current(tcb_t& thread)
    {
        using namespace artos::thread::internal;

        check(thread.ready.queue != nullptr, "current thread was not ready-queued");
        if (artos::kernel::internal::current_thread != nullptr
            && artos::kernel::internal::current_thread->state == state_t::running)
        {
            artos::kernel::internal::current_thread->state = state_t::ready;
        }
        thread.state = state_t::running;
        artos::kernel::internal::current_thread = &thread;
    }

    void make_not_ready(tcb_t& thread, state_t state)
    {
        check(!is_runnable(state), "non-ready transition used a runnable state");
        remove_ready(thread);
        thread.state = state;
    }

    void reset_scheduler()
    {
        using namespace artos::thread::internal;

        for (auto& queue : artos::kernel::internal::ready_queue)
        {
            queue.clear();
        }
        artos::kernel::internal::block_sleep_queue.clear();
        artos::kernel::internal::ready_bitmap.fill(0);

        for (auto& thread : tcbs)
        {
            if (thread.has_value())
            {
                check(thread->waiter.queue == nullptr, "previous test left a waiter queued");
                thread.reset();
            }
        }

        artos::kernel::internal::threads.fill(nullptr);
        artos::kernel::internal::thread_count = 0;
        artos::kernel::internal::current_thread = nullptr;
        artos::kernel::internal::next_thread = nullptr;
        now = {};
        pendsv_count = 0;
        cycles_remaining = 16000;
        (void)install(0, 15, state_t::ready);
    }

    void test_thread_name_is_owned_and_terminated()
    {
        artos::thread::name_t source {};
        source.fill('x');
        source.back() = '\0';
        tcb_t thread { std::string_view { source.data(), build_config::artos::max_thread_name_length },
                       priority(0),
                       &stacks[0],
                       &stacks[0],
                       1 };
        source[0] = 'y';
        check(thread.name.front() == 'x', "thread name borrowed the caller's storage");
        check(thread.name.back() == '\0', "maximum-length thread name was not terminated");
    }

    void test_highest_priority_ready_thread_wins()
    {
        reset_scheduler();
        auto& current = install(1, 8, state_t::running);
        (void)install(2, 5, state_t::ready);
        auto& highest = install(5, 2, state_t::ready);
        set_current(current);

        check(artos::kernel::internal::select_next() == &highest,
              "selector did not choose the highest-priority ready thread");
    }

    void test_equal_priority_threads_rotate()
    {
        reset_scheduler();
        auto& current = install(1, 4, state_t::running);
        auto& first = install(2, 4, state_t::ready);
        auto& second = install(3, 4, state_t::ready);
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
        auto& current = install(1, 7, state_t::running);
        auto& blocked = install(2, 0, state_t::indefinite);
        auto& suspended = install(3, 1, state_t::ready, true);
        auto& finished = install(4, 2, state_t::finished);
        set_current(current);

        check(blocked.ready.queue == nullptr, "blocked thread was ready-queued");
        check(suspended.ready.queue == nullptr, "suspended thread was ready-queued");
        check(finished.ready.queue == nullptr, "finished thread was ready-queued");
        check(artos::kernel::internal::select_next() == &current, "selector did not retain the only runnable thread");
    }

    void test_idle_is_only_the_fallback()
    {
        reset_scheduler();
        auto& idle = *tcbs[0];
        auto& current = install(1, 8, state_t::running);
        set_current(current);
        make_not_ready(current, state_t::indefinite);

        check(artos::kernel::internal::select_next() == &idle,
              "selector did not fall back to idle when all applications blocked");

        auto& application = install(2, 15, state_t::ready);
        check(artos::kernel::internal::select_next() == &application,
              "idle competed with a runnable lowest-priority application thread");
    }

    void test_near_tick_blocked_thread_idles_until_next_tick()
    {
        using namespace artos::thread::internal;

        reset_scheduler();
        auto& blocked = install(1, 8, state_t::running);
        auto& ready = install(2, 2, state_t::ready);
        set_current(blocked);
        blocked.state = state_t::indefinite;
        cycles_remaining = 100;

        artos::kernel::internal::sched();

        check(next_thread == threads[0], "near-tick blocked thread did not select idle");
        check(blocked.ready.queue == nullptr, "blocked thread remained ready-queued");
        check(pendsv_count == 1, "near-tick blocked thread did not request a handoff");
        check(ready.ready.queue != nullptr, "ready thread was removed while idling");

        current_thread = threads[0];
        current_thread->state = state_t::running;
        next_thread = nullptr;
        cycles_remaining = 16000;
        artos::kernel::internal::sched();
        check(next_thread == &ready, "next tick did not select the ready thread");
    }

    void test_sleep_deadlines_control_eligibility()
    {
        reset_scheduler();
        now = { .ticks = 100, .generation = 3 };
        auto& current = install(1, 8, state_t::running);
        auto& due = install(2, 2, state_t::sleeping);
        due.deadline = { .ticks = 100, .generation = 3 };
        auto& future = install(3, 0, state_t::sleeping);
        future.deadline = { .ticks = 101, .generation = 3 };
        artos::thread::internal::block_sleep_thread(&future);
        artos::thread::internal::block_sleep_thread(&due);
        set_current(current);

        artos::kernel::internal::sched();

        check(next_thread == &due, "scheduler did not select the due sleeper");
        check(due.state == state_t::ready, "due sleeper was not transitioned to ready");
        check(due.ready.queue != nullptr, "due sleeper was not inserted into a ready queue");
        check(due.block_sleep.queue == nullptr, "due sleeper remained in the deadline queue");
        check(future.state == state_t::sleeping, "future sleeper was readied early");
        check(future.ready.queue == nullptr, "future sleeper was inserted into a ready queue");
        check(future.block_sleep.queue != nullptr, "future sleeper was removed from the deadline queue");
    }

    void test_block_timeout_removes_waiter()
    {
        reset_scheduler();
        now = { .ticks = 50, .generation = 1 };
        auto& current = install(1, 9, state_t::running);
        auto& timed = install(2, 3, state_t::blocked);
        timed.deadline = now;
        artos::wait_queue_t queue;
        queue.push_back(timed.waiter);
        artos::thread::internal::block_sleep_thread(&timed);
        set_current(current);

        artos::kernel::internal::sched();

        check(next_thread == &timed, "scheduler did not select the timed-out thread");
        check(timed.state == state_t::ready, "timed-out thread was not made ready");
        check(timed.ready.queue != nullptr, "timed-out thread was not inserted into a ready queue");
        check(timed.block_sleep.queue == nullptr, "timed-out thread remained in the deadline queue");
        check(timed.waiter.wake == artos::wake_reason_t::timeout, "timed-out waiter recorded the wrong wake reason");
        check(timed.waiter.queue == nullptr && queue.empty(), "timed-out waiter remained in its wait queue");
    }

    void test_deadline_generation_wrap()
    {
        reset_scheduler();
        now = { .ticks = 1, .generation = 4 };
        auto& current = install(1, 8, state_t::running);
        auto& wrapped = install(2, 2, state_t::sleeping);
        wrapped.deadline = { .ticks = std::numeric_limits<artos::tick_t>::max(), .generation = 3 };
        auto& next_generation = install(3, 0, state_t::sleeping);
        next_generation.deadline = { .ticks = 1, .generation = 5 };
        artos::thread::internal::block_sleep_thread(&next_generation);
        artos::thread::internal::block_sleep_thread(&wrapped);
        set_current(current);

        artos::kernel::internal::sched();

        check(next_thread == &wrapped, "scheduler did not select the sleeper spanning tick rollover");
        check(wrapped.state == state_t::ready,
              "deadline immediately before tick rollover was not recognized as elapsed");
        check(next_generation.state == state_t::sleeping, "deadline in the next generation was treated as elapsed");
    }

    void test_sched_refreshes_pending_selection()
    {
        reset_scheduler();
        auto& current = install(1, 8, state_t::running);
        auto& higher = install(2, 2, state_t::ready);
        set_current(current);

        artos::kernel::internal::sched();
        check(next_thread == &higher, "sched did not publish its selection");
        check(pendsv_count == 1, "sched did not pend a required context switch");

        make_not_ready(higher, state_t::indefinite);
        artos::kernel::internal::sched();
        check(next_thread == &current, "sched did not refresh a stale pending destination");
        check(pendsv_count == 1, "sched pended a switch when the current thread remained selected");
    }

    void test_sched_preserves_startup_selection()
    {
        reset_scheduler();
        auto& selected = install(1, 2, state_t::ready);
        next_thread = &selected;

        artos::kernel::internal::sched();
        check(next_thread == &selected, "sched overwrote the startup selection before a context existed");
        check(pendsv_count == 0, "sched pended PendSV before the first context started");
    }

    void test_initial_selection_uses_the_highest_priority_thread()
    {
        reset_scheduler();
        (void)install(1, 5, state_t::ready);
        auto& highest = install(2, 2, state_t::ready);

        check(artos::kernel::internal::select_next() == &highest,
              "initial selection did not choose the highest-priority ready thread");
    }

    extern "C" bool                         artos_context_started() noexcept;
    extern "C" artos::thread::stack_word_t* artos_context_start() noexcept;
    extern "C" artos::thread::stack_word_t* artos_context_switch(artos::thread::stack_word_t* outgoing_sp) noexcept;

    void test_context_start_and_switch_state()
    {
        reset_scheduler();
        artos::thread::stack_word_t first_stack {};
        artos::thread::stack_word_t second_stack {};
        artos::thread::stack_word_t saved_stack {};
        auto&                       first = install(1, 4, state_t::ready);
        auto&                       second = install(2, 4, state_t::ready);
        first.sp = &first_stack;
        second.sp = &second_stack;
        next_thread = &first;

        check(!artos_context_started(), "context reported started before first dispatch");
        check(artos_context_start() == &first_stack, "context start returned the wrong stack");
        check(artos_context_started(), "context did not report started after first dispatch");
        check(first.state == state_t::running, "first context was not marked running");

#if defined(ARTOS_TEST_THREAD_STATS)
        check(first.statistics.execution_count == 1, "first dispatch was not counted");
        check(second.statistics.execution_count == 0, "undispatched thread was counted");
        check(first.statistics.runtime_history_cycles.size() == build_config::artos::runtime_history_bucket_count,
              "runtime history bucket count did not match configuration");
        check(first.statistics.runtime_history_cycles[0] == 0, "runtime history was not zero-initialized");
        first.statistics.runtime_history_cycles[0] = 1234;
        artos::thread::statistics_t copied_statistics {};
        first.get_statistics(copied_statistics);
        check(copied_statistics.runtime_history_cycles[0] == 1234, "runtime history was not copied");
#endif

        next_thread = &second;
        check(artos_context_switch(&saved_stack) == &second_stack, "context switch returned the wrong incoming stack");
        check(first.sp == &saved_stack && first.state == state_t::ready,
              "outgoing context was not saved and made ready");
        check(second.state == state_t::running, "incoming context was not marked running");

#if defined(ARTOS_TEST_THREAD_STATS)
        check(first.statistics.execution_count == 1, "outgoing thread was counted twice");
        check(second.statistics.execution_count == 1, "incoming dispatch was not counted");
#endif
    }

#if defined(ARTOS_TEST_PRIORITY_AGING)
    void test_priority_aging_promotes_waiter()
    {
        reset_scheduler();
        auto& current = install(1, 4, state_t::running);
        auto& waiting = install(2, 5, state_t::ready);
        set_current(current);

        artos::kernel::internal::sched();
        check(waiting.priority == priority(5) && waiting.ticks_at_priority == 1,
              "waiting thread aged before the configured interval");
        check(current.ticks_at_priority == 0, "running thread was aged");

        artos::kernel::internal::sched();
        check(waiting.priority == priority(4) && waiting.ticks_at_priority == 0,
              "waiting thread was not promoted at the configured interval");
        check(waiting.ready.queue == &ready_queue[4], "promoted thread was not moved to its new ready queue");

        artos::kernel::internal::sched();
        check(next_thread == &waiting, "promoted thread was not selected after reaching the running priority");
        check(waiting.priority == priority(5) && waiting.ticks_at_priority == 0,
              "selected thread did not regain its original priority");
        check(waiting.ready.queue == &ready_queue[5], "selected thread was not returned to its original ready queue");
    }

    void test_priority_aging_stops_at_highest_priority()
    {
        reset_scheduler();
        auto& highest = install(1, 0, state_t::ready);
        highest.age();
        highest.age();
        check(highest.priority == artos::thread::priority_t::high, "aging wrapped past the highest priority");
    }
#endif

    struct test_case_t
    {
        const char* name;
        void (*run)();
    };

    const std::array cases {
        test_case_t { "owned thread name", test_thread_name_is_owned_and_terminated },
        test_case_t { "highest priority", test_highest_priority_ready_thread_wins },
        test_case_t { "equal-priority rotation", test_equal_priority_threads_rotate },
        test_case_t { "ready-queue membership", test_non_runnable_threads_are_not_ready_queued },
        test_case_t { "idle fallback", test_idle_is_only_the_fallback },
        test_case_t { "near-tick blocked handoff", test_near_tick_blocked_thread_idles_until_next_tick },
        test_case_t { "sleep deadlines", test_sleep_deadlines_control_eligibility },
        test_case_t { "blocked timeout", test_block_timeout_removes_waiter },
        test_case_t { "deadline generation wrap", test_deadline_generation_wrap },
        test_case_t { "pending selection", test_sched_refreshes_pending_selection },
        test_case_t { "startup selection", test_sched_preserves_startup_selection },
        test_case_t { "initial selection", test_initial_selection_uses_the_highest_priority_thread },
        test_case_t { "context state transitions", test_context_start_and_switch_state },
#if defined(ARTOS_TEST_PRIORITY_AGING)
        test_case_t { "priority aging promotion", test_priority_aging_promotes_waiter },
        test_case_t { "priority aging limit", test_priority_aging_stops_at_highest_priority },
#endif
    };
}

#if defined(ARTOS_TEST_THREAD_STATS)
std::uint32_t board::stopwatch::count() noexcept
{
    return 0;
}
#endif

namespace artos::kernel {
    tick_time_t tick_time() noexcept
    {
        return now;
    }
}

namespace artos::port {
    std::uint32_t get_cycles_remaining() noexcept
    {
        return cycles_remaining;
    }

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
