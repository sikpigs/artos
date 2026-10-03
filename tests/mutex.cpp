#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <artos.h>
#include <mutex.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>

namespace {
    using namespace artos;
    using namespace artos::kernel::internal;
    using namespace artos::thread::internal;

    std::array<std::optional<tcb_t>, build_config::artos::max_threads> tcbs {};
    std::array<thread::stack_word_t, build_config::artos::max_threads> stacks {};
    tick_time_t                                                        now {};
    cpu::port::basepri_t                                               interrupt_mask {};
    bool                                                               interrupt_context = false;
    unsigned                                                           scheduling_requests = 0;
    unsigned                                                           block_count = 0;
    std::function<void(tcb_t&)>                                        on_block;

    void check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", message);
            std::exit(1);
        }
    }

    thread::priority_t priority(unsigned value)
    {
        return static_cast<thread::priority_t>(value);
    }

    tcb_t& install(std::size_t slot, unsigned priority_value)
    {
        auto& thread = tcbs[slot].emplace("mutex test", priority(priority_value), &stacks[slot], &stacks[slot], 1);
        (void)thread.make_thread_id(slot);
        threads[slot] = &thread;
        ++thread_count;
        make_thread_ready(&thread);
        return thread;
    }

    void run_as(tcb_t& thread)
    {
        if (current_thread && current_thread->state == thread::state_t::running)
        {
            current_thread->state = thread::state_t::ready;
        }
        check(thread.ready.queue != nullptr, "attempted to run a thread that was not ready");
        current_thread = &thread;
        thread.state = thread::state_t::running;
    }

    void setup()
    {
        // Slot zero is the idle fallback, matching the kernel registry.
        (void)install(0, 15);
        run_as(install(1, 9));
    }

    void expire(tcb_t& waiter)
    {
        check(waiter.state == thread::state_t::blocked, "timed waiter was not blocked");
        now = waiter.deadline;
        const critical_section_t cs {};
        sched(); // The real deadline scanner removes both wait registrations.
        check(waiter.waiter.wake == wake_reason_t::timeout, "deadline did not time out the waiter");
        check(waiter.waiter.queue == nullptr, "timeout left an object wait registration");
        check(waiter.block_sleep.queue == nullptr, "timeout left a deadline registration");
        check(waiter.waiting_mutex == nullptr, "timeout left a mutex dependency");
    }

    void test_uncontended()
    {
        mutex_t mutex;
        check(mutex.try_lock() == mutex_result_t::success, "uncontended try_lock failed");
        check(mutex.release() == mutex_result_t::success, "owner could not release");
        check(mutex.release() == mutex_result_t::not_locked, "double release was not rejected");
        check(mutex.lock(0) == mutex_result_t::success, "zero-wait acquisition of a free mutex failed");
        check(mutex.release() == mutex_result_t::success, "zero-wait owner could not release");
        check(mutex.lock(wait_forever) == mutex_result_t::success, "uncontended indefinite lock failed");
        check(mutex.release() == mutex_result_t::success, "indefinite owner could not release");
        check(block_count == 0, "uncontended lock blocked");
    }

    void test_contended_try_and_zero_wait()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        run_as(install(2, 1));
        check(mutex.try_lock() == mutex_result_t::timeout, "contended try_lock acquired the mutex");
        check(mutex.lock(0) == mutex_result_t::timeout, "contended zero-wait lock acquired the mutex");
        check(block_count == 0, "zero-wait acquisition blocked");
        check(owner.priority == priority(9), "zero-wait acquisition changed the owner's priority");
        run_as(owner);
        check(mutex.release() == mutex_result_t::success, "failed acquisition changed ownership");
    }

    void test_wrong_owner()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        run_as(install(2, 1));
        const auto before = scheduling_requests;
        check(mutex.release() == mutex_result_t::no_permission, "non-owner release succeeded");
        check(scheduling_requests == before, "failed release requested scheduling");
        check(mutex.try_lock() == mutex_result_t::timeout, "non-owner release unlocked the mutex");
        run_as(owner);
        check(mutex.release() == mutex_result_t::success, "non-owner release damaged ownership");
    }

    void test_nonrecursive_try_lock()
    {
        mutex_t mutex;
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        check(mutex.try_lock() == mutex_result_t::timeout, "try_lock recursively acquired the mutex");
        check(mutex.release() == mutex_result_t::success, "recursive attempt damaged ownership");
        check(mutex.release() == mutex_result_t::not_locked, "recursive attempt added an acquisition");
    }

    void test_release_without_waiters_does_not_schedule()
    {
        mutex_t    mutex;
        const auto before = scheduling_requests;
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        check(mutex.release() == mutex_result_t::success, "owner release failed");
        check(scheduling_requests == before, "release without waiters requested scheduling");
        check(mutex.try_lock() == mutex_result_t::success, "reacquisition failed");
        check(mutex.release() == mutex_result_t::success, "owner release failed");
        check(scheduling_requests == before, "repeated release without waiters requested scheduling");
    }

    void test_timed_wake()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        auto& waiter = install(2, 3);
        on_block = [&](tcb_t& blocked) {
            check(&blocked == &waiter, "wrong thread blocked");
            check(blocked.deadline == now + 5, "wrong timed deadline");
            run_as(owner);
            check(mutex.release() == mutex_result_t::success, "owner release failed");
            check(blocked.waiter.wake == wake_reason_t::signaled, "release did not signal waiter");
            check(blocked.waiting_mutex == nullptr, "release left a mutex dependency");
            check(blocked.block_sleep.queue == nullptr, "release retained the waiter's deadline");
        };
        run_as(waiter);
        check(mutex.lock(5) == mutex_result_t::success, "signaled timed waiter did not acquire");
        check(mutex.release() == mutex_result_t::success, "new owner could not release");
    }

    void test_indefinite_wake()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        auto& waiter = install(2, 3);
        on_block = [&](tcb_t& blocked) {
            check(blocked.state == thread::state_t::indefinite, "indefinite wait used a timed state");
            check(blocked.block_sleep.queue == nullptr, "indefinite wait registered a deadline");
            run_as(owner);
            const auto before = scheduling_requests;
            check(mutex.release() == mutex_result_t::success, "owner release failed");
            check(scheduling_requests > before, "release did not schedule the higher-priority waiter");
            check(next_thread == &blocked, "release did not select the higher-priority waiter");
            check(blocked.waiter.wake == wake_reason_t::signaled, "release did not wake waiter");
        };
        run_as(waiter);
        check(mutex.lock(wait_forever) == mutex_result_t::success, "signaled indefinite waiter did not acquire");
        check(mutex.release() == mutex_result_t::success, "new owner could not release");
    }

    void test_timeout_keeps_owner()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        auto& waiter = install(2, 12);
        on_block = [&](tcb_t& blocked) { expire(blocked); };
        run_as(waiter);
        check(mutex.lock(5) == mutex_result_t::timeout, "elapsed wait did not return timeout");
        check(mutex.try_lock() == mutex_result_t::timeout, "timeout unlocked the mutex");
        run_as(owner);
        check(mutex.release() == mutex_result_t::success, "timeout damaged ownership");
    }

    void test_inheritance_before_block()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        (void)install(2, 5); // Medium priority work must not delay the owner.
        auto& waiter = install(3, 1);
        on_block = [&](tcb_t& blocked) {
            check(owner.priority == priority(1), "owner was not boosted before waiter blocked");
            check(current_thread == &owner, "medium-priority work ran ahead of the inherited owner");
            check(owner.ready.queue == &ready_queue[1], "inherited owner is in the wrong ready queue");
            expire(blocked);
        };
        run_as(waiter);
        check(mutex.lock(5) == mutex_result_t::timeout, "elapsed wait did not return timeout");
    }

    void test_timeout_restores_priority()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        auto& waiter = install(2, 1);
        on_block = [&](tcb_t& blocked) { expire(blocked); };
        run_as(waiter);
        check(mutex.lock(5) == mutex_result_t::timeout, "elapsed wait did not return timeout");
        check(owner.priority == priority(9), "owner retained a donation from a timed-out waiter");
        check(owner.base_priority == priority(9), "timeout changed owner's base priority");
        check(owner.ready.queue == &ready_queue[9], "timeout left owner in the donated ready queue");
    }

    void test_release_restores_priority()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        // Seed an effective boost to isolate release from the donation path.
        set_effective_priority(&owner, priority(1));
        check(mutex.release() == mutex_result_t::success, "owner release failed");
        check(owner.priority == priority(9), "release did not restore owner's priority");
        check(owner.base_priority == priority(9), "release changed owner's base priority");
        check(owner.ready.queue == &ready_queue[9], "release left owner in donated ready queue");
    }

    void test_scheduler_keeps_inheritance()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        (void)install(2, 5);
        auto& waiter = install(3, 1);
        on_block = [&](tcb_t&) {
            {
                const critical_section_t cs {};
                sched();
            }
            check(owner.priority == priority(1), "scheduler reset a live mutex donation");
            check(owner.ready.queue == &ready_queue[1], "scheduler moved inherited owner to base priority queue");
            run_as(owner);
            check(mutex.release() == mutex_result_t::success, "owner release failed");
        };
        run_as(waiter);
        check(mutex.lock(wait_forever) == mutex_result_t::success, "donating waiter did not acquire");
        check(mutex.release() == mutex_result_t::success, "donating waiter could not release");
    }

    void test_deadline_rollover()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        now = { .ticks = std::numeric_limits<tick_t>::max() - 2, .generation = 4 };
        const auto expected = now + 5;
        auto&      waiter = install(2, 12);
        on_block = [&](tcb_t& blocked) {
            check(blocked.deadline == expected, "deadline lost rollover generation");
            expire(blocked);
        };
        run_as(waiter);
        check(mutex.lock(5) == mutex_result_t::timeout, "rollover deadline did not expire");
        run_as(owner);
        check(mutex.release() == mutex_result_t::success, "rollover timeout damaged ownership");
    }

    void test_retry_keeps_deadline()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        auto&      waiter = install(2, 12);
        auto&      barger = install(3, 9);
        const auto expected = now + 5;
        on_block = [&](tcb_t&) {
            run_as(owner);
            check(mutex.release() == mutex_result_t::success, "owner release failed");
            run_as(barger);
            check(mutex.try_lock() == mutex_result_t::success, "retry setup could not acquire the free mutex");
            now = now + 2;
            on_block = [&](tcb_t& blocked) {
                check(blocked.deadline == expected, "retry extended the original wait budget");
                expire(blocked);
            };
        };
        run_as(waiter);
        check(mutex.lock(5) == mutex_result_t::timeout, "retry did not honor the original deadline");
        check(block_count == 2, "test did not exercise a second wait");
        run_as(barger);
        check(mutex.release() == mutex_result_t::success, "retry damaged the second owner's mutex");
    }

    void test_isr_acquire_rejected()
    {
        mutex_t mutex;
        interrupt_context = true;
        check(mutex.try_lock() != mutex_result_t::success, "ISR acquired a thread-owned mutex");
        check(mutex.lock(0) != mutex_result_t::success, "ISR acquired through timed lock");
    }

    void test_isr_release_rejected()
    {
        mutex_t mutex;
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        interrupt_context = true;
        const auto before = scheduling_requests;
        check(mutex.release() == mutex_result_t::unsupported, "ISR release was not rejected as unsupported");
        check(scheduling_requests == before, "rejected ISR release requested scheduling");
        interrupt_context = false;
        check(mutex.release() == mutex_result_t::success, "ISR attempt damaged ownership");
    }

    void test_self_deadlock_rejected()
    {
        mutex_t mutex;
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        on_block = [](tcb_t&) { check(false, "owner blocked forever on its own nonrecursive mutex"); };
        const auto result = mutex.lock(wait_forever);
        check(result == mutex_result_t::deadlock_detected, "self-deadlock did not return an explicit rejection");
        check(block_count == 0, "self-deadlock detection registered a waiter");
        check(mutex.release() == mutex_result_t::success, "self-deadlock rejection damaged ownership");
    }

    void test_wait_cycle_rejected()
    {
        mutex_t first;
        mutex_t second;
        auto&   first_owner = *tcbs[1];
        check(first.try_lock() == mutex_result_t::success, "first owner acquisition failed");
        auto& second_owner = install(2, 9);
        run_as(second_owner);
        check(second.try_lock() == mutex_result_t::success, "second owner acquisition failed");
        on_block = [&](tcb_t&) {
            run_as(second_owner);
            // First owner is waiting for second. Waiting for first here would
            // complete a cycle; detection must reject before registering it.
            on_block = [](tcb_t&) { check(false, "second waiter completed a mutex wait cycle"); };
            const auto result = first.lock(wait_forever);
            check(result == mutex_result_t::deadlock_detected, "wait cycle did not return an explicit rejection");
            on_block = {};
            check(second_owner.waiter.queue == nullptr, "cycle rejection left a waiter registered");
            check(second.release() == mutex_result_t::success, "second owner could not break dependency");
        };
        run_as(first_owner);
        check(second.lock(wait_forever) == mutex_result_t::success, "first waiter did not acquire after rejection");
        check(second.release() == mutex_result_t::success, "first waiter could not release second mutex");
        check(first.release() == mutex_result_t::success, "cycle rejection damaged first mutex ownership");
    }

    void test_transitive_inheritance()
    {
        mutex_t first;
        mutex_t second;
        auto&   first_owner = *tcbs[1];
        first_owner.base_priority = priority(12);
        set_effective_priority(&first_owner, priority(12));
        check(first.try_lock() == mutex_result_t::success, "first owner acquisition failed");
        auto& second_owner = install(2, 9);
        run_as(second_owner);
        check(second.try_lock() == mutex_result_t::success, "second owner acquisition failed");
        on_block = [&](tcb_t&) {
            auto& high = install(3, 1);
            on_block = [&](tcb_t& blocked) {
                check(first_owner.priority == priority(1), "direct mutex owner did not inherit");
                check(second_owner.priority == priority(1), "donation did not propagate through blocked owner");
                expire(blocked);
            };
            run_as(high);
            check(first.lock(5) == mutex_result_t::timeout, "high-priority waiter did not time out");
            run_as(second_owner);
            check(second.release() == mutex_result_t::success, "second owner release failed");
        };
        run_as(first_owner);
        check(second.lock(wait_forever) == mutex_result_t::success, "first owner did not resume");
        check(second.release() == mutex_result_t::success, "resumed owner release failed");
        check(first.release() == mutex_result_t::success, "first owner release failed");
    }

    void test_transitive_timeout_restores_priority()
    {
        mutex_t first;
        mutex_t second;
        auto&   first_owner = *tcbs[1];
        first_owner.base_priority = priority(12);
        set_effective_priority(&first_owner, priority(12));
        check(first.try_lock() == mutex_result_t::success, "first owner acquisition failed");
        auto& second_owner = install(2, 9);
        run_as(second_owner);
        check(second.try_lock() == mutex_result_t::success, "second owner acquisition failed");
        on_block = [&](tcb_t&) {
            auto& high = install(3, 1);
            on_block = [&](tcb_t& blocked) {
                check(second_owner.priority == priority(1), "transitive donation setup failed");
                expire(blocked);
                auto expected = first_owner.base_priority;
#if defined(ARTOS_TEST_PRIORITY_AGING)
                if (first_owner.aged_priority)
                {
                    expected = std::min(expected, *first_owner.aged_priority);
                }
#endif
                check(first_owner.priority == expected, "direct owner retained an expired mutex donation");
                check(second_owner.priority == priority(9), "downstream owner retained an expired donation");
                check(second_owner.base_priority == priority(9), "timeout changed downstream owner's base priority");
                check(second_owner.ready.queue == &ready_queue[9], "downstream owner has wrong ready queue");
            };
            run_as(high);
            check(first.lock(5) == mutex_result_t::timeout, "high-priority waiter did not time out");
            run_as(second_owner);
            check(second.release() == mutex_result_t::success, "second owner release failed");
        };
        run_as(first_owner);
        check(second.lock(wait_forever) == mutex_result_t::success, "first owner did not resume");
        check(second.release() == mutex_result_t::success, "resumed owner release failed");
        check(first.release() == mutex_result_t::success, "first owner release failed");
    }

    void test_timed_out_owner_keeps_live_donation()
    {
        mutex_t first;
        mutex_t second;
        auto&   owner = *tcbs[1];
        check(first.try_lock() == mutex_result_t::success, "first owner acquisition failed");
        auto& second_owner = install(2, 12);
        run_as(second_owner);
        check(second.try_lock() == mutex_result_t::success, "second owner acquisition failed");
        on_block = [&](tcb_t& timed_owner) {
            auto& high = install(3, 1);
            on_block = [&](tcb_t& blocked) {
                check(owner.priority == priority(1), "live donation setup failed");
                expire(timed_owner);
                check(owner.priority == priority(1), "owner's own timeout discarded a live mutex donation");
                check(owner.base_priority == priority(9), "owner's own timeout changed its base priority");
                check(owner.ready.queue == &ready_queue[1], "timed-out owner has wrong ready queue");
                check(blocked.waiter.queue != nullptr, "donating waiter was unexpectedly removed");
                run_as(owner);
                check(first.release() == mutex_result_t::success, "owner could not wake donating waiter");
            };
            run_as(high);
            check(first.lock(wait_forever) == mutex_result_t::success, "donating waiter did not acquire");
            check(first.release() == mutex_result_t::success, "donating waiter could not release");
        };
        run_as(owner);
        check(second.lock(5) == mutex_result_t::timeout, "owner's wait did not time out");
        run_as(second_owner);
        check(second.release() == mutex_result_t::success, "second owner could not release");
    }

#if defined(ARTOS_TEST_PRIORITY_AGING)
    void exercise_nonrunnable_thread_aging(thread::state_t state)
    {
        auto& inactive = install(2, 6);
        auto* queue = inactive.ready.queue;
        queue->remove(inactive.ready);
        if (queue->empty())
        {
            clear_priority_ready(inactive.priority);
        }
        inactive.state = state;
        // This isolates the scheduler's eligibility filter, without a deadline
        // or an object wake making the registered thread runnable during sched().
        for (tick_t tick = 0; tick < 3 * build_config::artos::scheduler.priority_aging_ticks.value(); ++tick)
        {
            const critical_section_t cs {};
            sched();
        }
        check(inactive.priority == inactive.base_priority, "scheduler aged a nonrunnable thread");
        check(inactive.ticks_at_priority == 0, "nonrunnable thread accumulated aging credit");
        check(!inactive.aged_priority, "nonrunnable thread acquired an aging contribution");
        check(inactive.ready.queue == nullptr, "scheduler queued a nonrunnable thread");
    }

    void test_timed_blocked_thread_does_not_age()
    {
        exercise_nonrunnable_thread_aging(thread::state_t::blocked);
    }

    void test_indefinite_thread_does_not_age()
    {
        exercise_nonrunnable_thread_aging(thread::state_t::indefinite);
    }

    void test_sleeping_thread_does_not_age()
    {
        exercise_nonrunnable_thread_aging(thread::state_t::sleeping);
    }

    void test_finished_thread_does_not_age()
    {
        exercise_nonrunnable_thread_aging(thread::state_t::finished);
    }

    void test_scheduler_preserves_waiter_donation()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        owner.base_priority = priority(12);
        set_effective_priority(&owner, owner.base_priority);
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        auto& waiter = install(2, 9);
        on_block = [&](tcb_t& blocked) {
            // Keep the owner current and select higher-priority independent work.
            // Neither is aged; any change to the blocked waiter's priority must
            // still be reflected in the owner's effective priority.
            run_as(owner);
            (void)install(3, 1);
            for (tick_t tick = 0; tick < 2 * build_config::artos::scheduler.priority_aging_ticks.value(); ++tick)
            {
                const critical_section_t cs {};
                sched();
                check(owner.priority <= waiter.priority, "mutex owner lost the blocked waiter's current donation");
            }
            expire(blocked);
        };
        run_as(waiter);
        check(mutex.lock(10) == mutex_result_t::timeout, "waiter did not time out");
        run_as(owner);
        check(mutex.release() == mutex_result_t::success, "owner release failed");
    }

    void test_new_owner_inherits_remaining_waiter()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        owner.base_priority = priority(12);
        set_effective_priority(&owner, owner.base_priority);
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        auto& remaining = install(2, 3);
        on_block = [&](tcb_t&) {
            auto& acquiring = install(3, 9);
            // Aging makes this otherwise less urgent thread the next waiter
            // selected for wakeup. Its aging is reset before it acquires.
            for (tick_t tick = 0; tick < 7 * build_config::artos::scheduler.priority_aging_ticks.value(); ++tick)
            {
                acquiring.age();
            }
            check(acquiring.priority == priority(2), "acquiring waiter was not aged as expected");
            on_block = [&](tcb_t& blocked) {
                run_as(owner);
                check(mutex.release() == mutex_result_t::success, "original owner release failed");
                check(blocked.waiter.wake == wake_reason_t::signaled, "aged waiter was not selected for wakeup");
                check(remaining.waiter.queue != nullptr, "remaining waiter was unexpectedly removed");
                check(acquiring.priority == acquiring.base_priority, "acquiring waiter's aging did not reset");
            };
            run_as(acquiring);
            check(mutex.lock(wait_forever) == mutex_result_t::success, "aged waiter did not acquire");
            check(acquiring.priority == remaining.priority, "new owner did not inherit from the remaining waiter");
            check(acquiring.ready.queue == &ready_queue[std::to_underlying(remaining.priority)],
                  "new owner is in the wrong inherited ready queue");
            check(acquiring.base_priority == priority(9), "acquisition changed new owner's base priority");
            check(mutex.release() == mutex_result_t::success, "new owner could not wake remaining waiter");
        };
        run_as(remaining);
        check(mutex.lock(wait_forever) == mutex_result_t::success, "remaining waiter did not acquire");
        check(mutex.release() == mutex_result_t::success, "remaining waiter could not release");
    }

    void test_aged_owner_keeps_live_donation()
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        // Age a ready owner using the production aging code, before donating.
        run_as(install(2, 5));
        for (tick_t tick = 0; tick < build_config::artos::scheduler.priority_aging_ticks.value(); ++tick)
        {
            owner.age();
        }
        check(owner.priority == priority(8), "owner did not age");
        auto& high = install(3, 1);
        on_block = [&](tcb_t& blocked) {
            check(owner.priority == priority(1), "aging reset discarded a live mutex donation");
            check(owner.ready.queue == &ready_queue[1], "aged inherited owner has wrong ready queue");
            run_as(owner);
            check(mutex.release() == mutex_result_t::success, "aged owner release failed");
            check(owner.priority == priority(9), "aged owner did not restore its original priority");
            check(blocked.waiting_mutex == nullptr, "release retained the donating waiter's dependency");
        };
        run_as(high);
        check(mutex.lock(wait_forever) == mutex_result_t::success, "donating waiter did not acquire");
        check(mutex.release() == mutex_result_t::success, "donating waiter could not release");
    }

    void exercise_donation_before_aging(bool end_with_timeout)
    {
        mutex_t mutex;
        auto&   owner = *tcbs[1];
        check(mutex.try_lock() == mutex_result_t::success, "owner acquisition failed");
        auto& waiter = install(2, 3);
        on_block = [&](tcb_t& blocked) {
            check(owner.priority == priority(3), "owner did not receive the initial donation");

            // Give another thread the CPU, then age the ready, donated owner.
            run_as(install(3, 5));
            for (tick_t tick = 0; tick < build_config::artos::scheduler.priority_aging_ticks.value(); ++tick)
            {
                owner.age();
            }
            check(owner.priority < priority(3), "donated owner did not receive an aging boost");
            {
                const critical_section_t cs {};
                sched(); // Selecting the owner removes its aging boost.
            }
            check(next_thread == &owner, "scheduler did not select the aged owner");
            check(owner.priority == priority(3), "aging reset did not preserve the live donation");
            check(owner.ready.queue == &ready_queue[3], "inherited owner has wrong ready queue after aging reset");
            check(blocked.waiter.queue != nullptr, "aging reset unexpectedly removed the donating waiter");

            if (end_with_timeout)
            {
                expire(blocked);
                check(owner.priority == priority(9), "donation timeout after aging did not restore normal priority");
            }
            else
            {
                run_as(owner);
                check(mutex.release() == mutex_result_t::success, "owner release failed");
                check(owner.priority == priority(9),
                      "release after donation then aging did not restore normal priority");
                check(blocked.waiting_mutex == nullptr, "release retained the donating waiter's dependency");
            }
            check(owner.ready.queue == &ready_queue[9], "normal priority restoration left owner in wrong ready queue");
        };
        run_as(waiter);
        if (end_with_timeout)
        {
            check(mutex.lock(5) == mutex_result_t::timeout, "donating waiter did not time out");
            run_as(owner);
            check(mutex.release() == mutex_result_t::success, "timeout damaged ownership");
        }
        else
        {
            check(mutex.lock(wait_forever) == mutex_result_t::success, "donating waiter did not acquire");
            check(mutex.release() == mutex_result_t::success, "donating waiter could not release");
        }
    }

    void test_donation_then_aging_release_restores_priority()
    {
        exercise_donation_before_aging(false);
    }

    void test_donation_then_aging_timeout_restores_priority()
    {
        exercise_donation_before_aging(true);
    }
#endif

    void test_multiple_owned_mutexes()
    {
        mutex_t first;
        mutex_t second;
        auto&   owner = *tcbs[1];
        check(first.try_lock() == mutex_result_t::success, "first mutex acquisition failed");
        check(second.try_lock() == mutex_result_t::success, "second mutex acquisition failed");
        auto& high = install(2, 1);
        auto& lower = install(3, 3);
        on_block = [&](tcb_t&) {
            on_block = [&](tcb_t&) {
                run_as(owner);
                check(first.release() == mutex_result_t::success, "first mutex release failed");
                check(owner.priority == priority(3), "release did not retain only the remaining mutex donation");
                check(owner.ready.queue == &ready_queue[3], "remaining donation has wrong ready queue");
                check(second.release() == mutex_result_t::success, "second mutex release failed");
                check(owner.priority == priority(9), "final release did not restore base priority");
            };
            run_as(lower);
            check(second.lock(wait_forever) == mutex_result_t::success, "lower-priority waiter did not acquire");
            check(second.release() == mutex_result_t::success, "lower-priority owner release failed");
        };
        run_as(high);
        check(first.lock(wait_forever) == mutex_result_t::success, "high-priority waiter did not acquire");
        check(first.release() == mutex_result_t::success, "high-priority owner release failed");
    }
}

namespace cpu::port {
    basepri_t irq_save_raise_mask(basepri_t priority_value) noexcept
    {
        const auto previous = interrupt_mask;
        if (interrupt_mask == 0 || priority_value < interrupt_mask)
        {
            interrupt_mask = priority_value;
        }
        return previous;
    }

    void irq_restore_mask(basepri_t previous) noexcept
    {
        interrupt_mask = previous;
        if (interrupt_mask != 0 || !current_thread
            || (current_thread->state != thread::state_t::blocked
                && current_thread->state != thread::state_t::indefinite))
        {
            return;
        }

        // Simulate the context switch only after the blocking critical section
        // releases BASEPRI. The production blocking, wake and scheduler code is
        // linked unchanged; a scenario runs other threads until the waiter wakes.
        ++block_count;
        check(block_count <= 4, "mutex wait did not make progress");
        check(static_cast<bool>(on_block), "unexpected blocking call");
        auto  handler = std::exchange(on_block, {});
        auto& waiter = *static_cast<tcb_t*>(current_thread);
        check(next_thread != nullptr && next_thread != &waiter, "scheduler did not select another thread");
        run_as(*static_cast<tcb_t*>(next_thread));
        handler(waiter);
        check(waiter.waiter.queue == nullptr, "scenario resumed a waiter without waking it");
        run_as(waiter);
    }

    bool is_in_irq() noexcept
    {
        return interrupt_context;
    }
}

namespace artos::port {
    void pendsvset() noexcept
    {
        ++scheduling_requests;
    }

    std::uint32_t get_cycles_remaining() noexcept
    {
        return 16000;
    }
}

namespace artos::kernel {
    tick_time_t tick_time() noexcept
    {
        return now;
    }

    namespace locked {
        tick_time_t tick_time() noexcept
        {
            return now;
        }
    }
}

namespace artos::thread {
    id_t get_current_thread() noexcept
    {
        return current_thread ? current_thread->id : invalid_id;
    }
}

namespace board::stopwatch {
    std::uint32_t count() noexcept
    {
        return 0;
    }
}

int main()
{
    struct test_t
    {
        const char* name;
        void (*run)();
    };

    const std::array tests {
        test_t { "uncontended", test_uncontended },
        test_t { "contended_try_and_zero_wait", test_contended_try_and_zero_wait },
        test_t { "wrong_owner", test_wrong_owner },
        test_t { "nonrecursive_try_lock", test_nonrecursive_try_lock },
        test_t { "release_without_waiters_does_not_schedule", test_release_without_waiters_does_not_schedule },
        test_t { "timed_wake", test_timed_wake },
        test_t { "indefinite_wake", test_indefinite_wake },
        test_t { "timeout_keeps_owner", test_timeout_keeps_owner },
        test_t { "inheritance_before_block", test_inheritance_before_block },
        test_t { "timeout_restores_priority", test_timeout_restores_priority },
        test_t { "release_restores_priority", test_release_restores_priority },
        test_t { "scheduler_keeps_inheritance", test_scheduler_keeps_inheritance },
        test_t { "deadline_rollover", test_deadline_rollover },
        test_t { "retry_keeps_deadline", test_retry_keeps_deadline },
        test_t { "isr_acquire_rejected", test_isr_acquire_rejected },
        test_t { "isr_release_rejected", test_isr_release_rejected },
        test_t { "self_deadlock_rejected", test_self_deadlock_rejected },
        test_t { "wait_cycle_rejected", test_wait_cycle_rejected },
        test_t { "transitive_inheritance", test_transitive_inheritance },
        test_t { "transitive_timeout_restores_priority", test_transitive_timeout_restores_priority },
        test_t { "timed_out_owner_keeps_live_donation", test_timed_out_owner_keeps_live_donation },
#if defined(ARTOS_TEST_PRIORITY_AGING)
        test_t { "timed_blocked_thread_does_not_age", test_timed_blocked_thread_does_not_age },
        test_t { "indefinite_thread_does_not_age", test_indefinite_thread_does_not_age },
        test_t { "sleeping_thread_does_not_age", test_sleeping_thread_does_not_age },
        test_t { "finished_thread_does_not_age", test_finished_thread_does_not_age },
        test_t { "scheduler_preserves_waiter_donation", test_scheduler_preserves_waiter_donation },
        test_t { "new_owner_inherits_remaining_waiter", test_new_owner_inherits_remaining_waiter },
        test_t { "aged_owner_keeps_live_donation", test_aged_owner_keeps_live_donation },
        test_t { "donation_then_aging_release_restores_priority", test_donation_then_aging_release_restores_priority },
        test_t { "donation_then_aging_timeout_restores_priority", test_donation_then_aging_timeout_restores_priority },
#endif
        test_t { "multiple_owned_mutexes", test_multiple_owned_mutexes },
    };

    // Isolate scenarios so an assertion in production code cannot hide the
    // remaining results. A signal is a failure, not an expected test outcome.
    const rlimit core_limit { 0, 0 };
    check(setrlimit(RLIMIT_CORE, &core_limit) == 0, "could not disable core dumps");
    unsigned failures = 0;
    for (const auto& test : tests)
    {
        std::fflush(nullptr);
        const auto child = fork();
        check(child >= 0, "fork failed");
        if (child == 0)
        {
            setup();
            test.run();
            std::exit(0);
        }
        int status = 0;
        check(waitpid(child, &status, 0) == child, "waitpid failed");
        const bool passed = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        std::printf("%s: %s\n", passed ? "PASS" : "FAIL", test.name);
        failures += !passed;
    }
    std::printf("%zu tests, %u failed\n", tests.size(), failures);
    return failures == 0 ? 0 : 1;
}
