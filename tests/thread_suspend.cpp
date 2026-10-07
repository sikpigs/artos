#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

#include <artos.h>
#include <artos_port.h>
#include <cpu_port.h>
#include <mutex.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>

namespace {
    using namespace artos;
    using namespace artos::kernel::internal;
    using namespace artos::thread::internal;

    using storage_t = thread::storage_t<thread::minimum_stack_bytes>;
    std::array<storage_t, build_config::artos::max_threads> storage {};
    tick_time_t                                             now {};
    cpu::port::basepri_t                                    interrupt_mask {};
    bool                                                    in_irq {};
    unsigned                                                pendsv_count {};
    std::uint32_t                                           cycles_remaining { 16000 };
    std::function<void(tcb_t&)>                             on_suspend;

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

    void entry(void*)
    {
    }

    tcb_t& lookup(thread::id_t id)
    {
        return *static_cast<tcb_t*>(threads[tcb_t::id_layout::extract_id(id)]);
    }

    tcb_t& create(std::size_t slot, unsigned priority_value, thread::create_options_t options = thread::make_ready)
    {
        const auto result = thread::create(entry, "worker", nullptr, priority(priority_value), storage[slot], options);
        check(result.has_value(), "static thread creation failed");
        return lookup(*result);
    }

    void run_as(tcb_t& target)
    {
        if (current_thread && current_thread->state == thread::state_t::running)
        {
            current_thread->state = thread::state_t::ready;
        }
        current_thread = &target;
        target.state = thread::state_t::running;
    }

    void setup()
    {
        for (auto& queue : ready_queue)
        {
            queue.clear();
        }
        block_sleep_queue.clear();
        ready_bitmap.fill(0);
        threads.fill(nullptr);
        thread_count = 0;
        current_thread = nullptr;
        next_thread = nullptr;
        interrupt_mask = 0;
        in_irq = false;
        pendsv_count = 0;
        cycles_remaining = 16000;
        now = {};
        on_suspend = {};
        const auto idle = set_idle(entry, storage[0]);
        check(idle.has_value(), "idle creation failed");
        run_as(create(1, 9));
        (void)create(2, 5);
    }

    void reset_selection()
    {
        pendsv_count = 0;
        next_thread = nullptr;
    }

    void block(tcb_t& target, wait_queue_t& waiters, bool timed)
    {
        auto* controller = static_cast<tcb_t*>(current_thread);
        run_as(target);
        {
            const critical_section_t cs {};
            if (timed)
            {
                kernel::locked::block_current(waiters, now + 10);
            }
            else
            {
                kernel::locked::block_current(waiters, wait_forever);
            }
        }
        run_as(*controller);
        reset_selection();
    }

    void test_idle_identity()
    {
        setup();
        check(threads[0]->id == 0, "idle TCB was not assigned the reserved ID zero");
        check(!threads[0]->suspended && !threads[0]->ready.queue,
              "idle was suspended or inserted into an application ready queue");
    }

    void test_static_creation_options()
    {
        setup();
        auto& ready = lookup(2);
        check(!ready.suspended && ready.ready.queue, "default creation did not make the thread ready");
        auto& target = create(3, 3, thread::create_suspended);
        check(target.suspended && target.state == thread::state_t::ready && !target.ready.queue,
              "suspended creation made the thread schedulable");
        check(select_next() != &target, "scheduler selected a newly created suspended thread");
        thread::snapshot_array_t snapshot {};
        const auto               count = thread::get_snapshot(snapshot);
        check(count == 4 && snapshot[3].id == target.id && snapshot[3].suspended,
              "snapshot did not expose the initial suspension");
        reset_selection();
        check(thread::resume(target.id) == thread::resume_result_t::success, "initial resume failed");
        check(!target.suspended && target.ready.queue && next_thread == &target && pendsv_count == 1,
              "initial resume did not make and select the higher-priority thread");
        reset_selection();
        check(thread::resume(target.id) == thread::resume_result_t::success, "repeat resume failed");
        check(!next_thread && pendsv_count == 0, "repeat resume requested scheduling");
        auto it = target.ready.queue->begin();
        ++it;
        check(it == target.ready.queue->end(), "repeat resume duplicated ready membership");
        auto& explicit_ready = create(4, 12, thread::make_ready);
        check(!explicit_ready.suspended && explicit_ready.ready.queue,
              "explicit ready creation did not make the thread ready");
    }

    void test_dynamic_creation_options()
    {
        setup();
        const auto result = thread::create<thread::minimum_stack_bytes>(
            entry, "dynamic", nullptr, priority(3), thread::create_suspended);
        check(result.has_value(), "dynamic suspended creation failed");
        auto& target = lookup(*result);
        check(target.suspended && !target.ready.queue && target.on_exit,
              "dynamic creation lost the suspension or cleanup callback");
        check(thread::resume(target.id) == thread::resume_result_t::success && target.ready.queue,
              "dynamic thread could not be resumed");
        check(thread::kill(target.id) == thread::kill_result_t::success, "dynamic thread could not be killed");
        const auto cleanup = target.on_exit;
        auto*      context = target.on_exit_ctx;
        threads[tcb_t::id_layout::extract_id(target.id)] = nullptr;
        cleanup(context);
    }

    void test_dynamic_creation_error()
    {
        setup();
        const std::string too_long(build_config::artos::max_thread_name_length + 1, 'x');
        const auto        before = thread_count;
        const auto        result = thread::create<thread::minimum_stack_bytes>(
            entry, too_long, nullptr, priority(3), thread::create_suspended);
        check(!result.has_value(), "dynamic creation converted ENAMETOOLONG into a successful thread ID");
        check(result.error() == ENAMETOOLONG && thread_count == before,
              "failed dynamic creation changed the registry or returned the wrong error");
    }

    void test_dynamic_creation_capacity_error()
    {
        setup();
        for (std::size_t slot = 3; slot < threads.size(); ++slot)
        {
            (void)create(slot, 12, thread::create_suspended);
        }
        const auto result = thread::create<thread::minimum_stack_bytes>(entry, "full", nullptr, priority(3));
        check(!result.has_value(), "dynamic creation converted EAGAIN into a successful thread ID");
        check(result.error() == EAGAIN, "full registry returned the wrong creation error");
    }

    void test_suspend_ready()
    {
        setup();
        auto& target = lookup(2);
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "ready suspension failed");
        check(target.suspended && !target.ready.queue && get_highest_set_priority() == priority(9),
              "ready suspension left membership or the priority bitmap set");
        check(select_next() == current_thread, "scheduler selected the suspended thread");
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "repeat suspension failed");
        auto& peer = create(3, 5);
        check(thread::resume(target.id) == thread::resume_result_t::success, "ready resume failed");
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "second suspension failed");
        check(get_highest_set_priority() == priority(5) && select_next() == &peer,
              "suspending a peer cleared another thread's priority bitmap bit");
    }

    void self_suspend(bool near_tick)
    {
        setup();
        auto& target = lookup(1);
        check(thread::suspend(lookup(2).id) == thread::suspend_result_t::success, "setup suspension failed");
        if (near_tick)
        {
            cycles_remaining = 0;
        }
        bool acknowledged = false;
        on_suspend = [&](tcb_t& outgoing) {
            check(&outgoing == &target && outgoing.suspended && !outgoing.ready.queue,
                  "self suspension left the current thread runnable");
            check(next_thread == threads[0], "self suspension did not select idle as the fallback");
            run_as(*static_cast<tcb_t*>(next_thread));
            next_thread = nullptr;
            cycles_remaining = 16000;
            check(thread::resume(outgoing.id) == thread::resume_result_t::success, "controller could not resume self");
            check(next_thread == &outgoing && outgoing.ready.queue, "self resume did not restore ready membership");
            run_as(outgoing);
            acknowledged = true;
        };
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "self suspension failed");
        check(acknowledged && current_thread == &target && !target.suspended,
              "self suspension returned before the simulated controller resumed it");
    }

    void test_self_suspend()
    {
        self_suspend(false);
    }

    void test_self_suspend_near_tick()
    {
        self_suspend(true);
    }

    void test_resume_priorities()
    {
        for (const auto value : { 3U, 9U, 12U })
        {
            setup();
            auto& target = lookup(2);
            set_effective_priority(&target, priority(value));
            check(thread::suspend(target.id) == thread::suspend_result_t::success, "priority setup suspension failed");
            reset_selection();
            check(thread::resume(target.id) == thread::resume_result_t::success, "priority resume failed");
            check(target.ready.queue && !target.suspended, "priority resume did not restore the thread");
            check((pendsv_count != 0) == (value < 9), "resume used the wrong priority comparison");
            check((next_thread != nullptr) == (value < 9), "resume scheduled for equal or lower priority");
        }
    }

    void test_resume_idle_at_low_priority()
    {
        setup();
        auto& target = lookup(2);
        set_effective_priority(&target, thread::priority_t::low);
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "idle setup suspension failed");
        run_as(*static_cast<tcb_t*>(threads[0]));
        reset_selection();
        check(thread::resume(target.id) == thread::resume_result_t::success, "idle resume failed");
        check(target.ready.queue && !target.suspended, "idle resume did not restore readiness");
        check(!next_thread && pendsv_count == 0, "equal-priority resume requested preemption");
        sched();
        check(next_thread && next_thread != threads[0], "scheduler selected idle despite a ready application thread");
    }

    void test_resume_before_start()
    {
        setup();
        auto& target = lookup(2);
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "pre-start suspension failed");
        current_thread = nullptr;
        reset_selection();
        check(thread::resume(target.id) == thread::resume_result_t::success && target.ready.queue,
              "pre-start resume did not enqueue the thread");
        check(!next_thread && pendsv_count == 0, "pre-start resume requested a context switch");
    }

    void test_resume_preserves_wait()
    {
        for (const bool timed : { false, true })
        {
            setup();
            wait_queue_t waiters {};
            auto&        target = lookup(2);
            block(target, waiters, timed);
            const auto state = target.state;
            const auto deadline = target.deadline;
            check(thread::suspend(target.id) == thread::suspend_result_t::success, "blocked suspension failed");
            check(target.waiter.queue == &waiters && target.deadline == deadline, "suspension changed a pending wait");
            check(thread::resume(target.id) == thread::resume_result_t::success, "blocked resume failed");
            check(target.state == state && target.waiter.queue == &waiters && !target.ready.queue,
                  "resume completed a pending wait");
            check(!next_thread && pendsv_count == 0, "resume scheduled a still-blocked thread");
            check(kernel::locked::wake_one(waiters), "resumed blocked thread could not be signaled");
            check(target.ready.queue && !target.block_sleep.queue, "signal did not complete the resumed wait");
        }
    }

    void test_signal_during_suspension()
    {
        setup();
        wait_queue_t waiters {};
        auto&        target = lookup(2);
        block(target, waiters, true);
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "signal setup suspension failed");
        check(!kernel::locked::wake_one(waiters), "signal reported a suspended thread as runnable");
        check(target.state == thread::state_t::ready && target.suspended && !target.ready.queue,
              "signal scheduled the suspended waiter or failed to record readiness");
        check(!target.waiter.queue && !target.block_sleep.queue && target.waiter.wake == wake_reason_t::signaled,
              "signal failed to complete suspended wait registrations");
        check(thread::resume(target.id) == thread::resume_result_t::success && target.ready.queue,
              "resume lost the completed signal");
        check(next_thread == &target && pendsv_count == 1, "signaled resumed thread was not scheduled");
    }

    void test_wake_all_during_suspension()
    {
        setup();
        wait_queue_t waiters {};
        auto&        first = lookup(2);
        auto&        second = create(3, 3);
        block(first, waiters, true);
        block(second, waiters, false);
        check(thread::suspend(first.id) == thread::suspend_result_t::success
                  && thread::suspend(second.id) == thread::suspend_result_t::success,
              "wake-all setup suspension failed");
        check(!kernel::locked::wake_all(waiters, wake_reason_t::cancelled), "wake-all scheduled a suspended waiter");
        for (auto* target : { &first, &second })
        {
            check(target->state == thread::state_t::ready && target->suspended && !target->ready.queue
                      && !target->waiter.queue && !target->block_sleep.queue
                      && target->waiter.wake == wake_reason_t::cancelled,
                  "wake-all lost a suspended wait's cancellation");
            check(thread::resume(target->id) == thread::resume_result_t::success && target->ready.queue,
                  "cancelled suspended waiter could not resume");
        }
    }

    void test_timeout_during_suspension()
    {
        setup();
        wait_queue_t waiters {};
        auto&        target = lookup(2);
        block(target, waiters, true);
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "timeout setup suspension failed");
        now = target.deadline;
        sched();
        check(target.state == thread::state_t::ready && target.suspended && !target.ready.queue,
              "expired timeout scheduled a suspended thread");
        check(!target.waiter.queue && !target.block_sleep.queue && target.waiter.wake == wake_reason_t::timeout,
              "suspended timeout did not remove registrations or record timeout");
        reset_selection();
        check(thread::resume(target.id) == thread::resume_result_t::success && next_thread == &target,
              "timed-out suspended waiter could not resume");
    }

    void test_sleep_during_suspension()
    {
        setup();
        auto& target = lookup(2);
        auto* controller = static_cast<tcb_t*>(current_thread);
        run_as(target);
        thread::sleep_until(now + 10);
        run_as(*controller);
        reset_selection();
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "sleep suspension failed");
        check(target.state == thread::state_t::sleeping && target.block_sleep.queue, "suspension cancelled sleep");
        now = target.deadline;
        sched();
        check(target.state == thread::state_t::ready && target.suspended && !target.ready.queue
                  && !target.block_sleep.queue,
              "sleep deadline scheduled a suspended sleeper or failed to expire");
        check(thread::resume(target.id) == thread::resume_result_t::success && next_thread == &target,
              "expired sleeper did not become runnable on resume");
    }

    void test_mutex_ownership_survives_suspension()
    {
        setup();
        mutex_t mutex {};
        auto&   owner = lookup(2);
        auto*   controller = static_cast<tcb_t*>(current_thread);
        run_as(owner);
        check(mutex.try_lock() == mutex_result_t::success, "mutex setup lock failed");
        run_as(*controller);
        check(thread::suspend(owner.id) == thread::suspend_result_t::success, "mutex owner suspension failed");
        check(!owner.mutexes.empty() && mutex.try_lock() == mutex_result_t::timeout,
              "suspension released an owned mutex");
        check(mutex.release() == mutex_result_t::no_permission, "controller released the suspended owner's mutex");
        check(thread::resume(owner.id) == thread::resume_result_t::success, "mutex owner resume failed");
        run_as(owner);
        check(mutex.release() == mutex_result_t::success, "resumed owner lost mutex ownership");
    }

    void test_isr_resume()
    {
        for (const auto value : { 3U, 9U, 12U })
        {
            setup();
            auto& target = lookup(2);
            set_effective_priority(&target, priority(value));
            check(thread::suspend(target.id) == thread::suspend_result_t::success, "ISR setup suspension failed");
            reset_selection();
            in_irq = true;
            bool woken = false;
            check(thread::resume_from_isr(target.id, &woken) == thread::resume_result_t::success, "ISR resume failed");
            check(woken == (value < 9), "ISR resume reported the wrong priority wake flag");
            check(!target.suspended && target.ready.queue && !next_thread && pendsv_count == 0,
                  "ISR resume requested scheduling or failed to make the thread runnable");
            woken = true;
            check(thread::resume_from_isr(target.id, &woken) == thread::resume_result_t::success && woken,
                  "repeat ISR resume cleared an accumulated wake flag");
        }
    }

    void test_isr_resume_blocked()
    {
        setup();
        wait_queue_t waiters {};
        auto&        target = lookup(2);
        block(target, waiters, true);
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "ISR blocked suspension failed");
        in_irq = true;
        bool woken = false;
        check(thread::resume_from_isr(target.id, &woken) == thread::resume_result_t::success && !woken,
              "ISR resume reported a still-blocked thread as woken");
        check(target.waiter.queue == &waiters && target.block_sleep.queue && !target.ready.queue,
              "ISR resume cancelled a pending wait");
        check(kernel::locked::wake_one(waiters), "resumed ISR waiter could not be signaled");
    }

    void test_invalid_and_isr_calls()
    {
        setup();
        auto& target = lookup(2);
        for (const auto id : { thread::invalid_id,
                               thread::id_t { 0 },
                               target.id + tcb_t::id_layout::generation_increment,
                               thread::id_t { 7 } })
        {
            check(thread::suspend(id) == thread::suspend_result_t::invalid_thread,
                  "suspend accepted an invalid, stale, empty, or idle ID");
            check(thread::resume(id) == thread::resume_result_t::invalid_thread,
                  "resume accepted an invalid, stale, empty, or idle ID");
            bool woken = true;
            in_irq = true;
            check(thread::resume_from_isr(id, &woken) == thread::resume_result_t::invalid_thread && woken,
                  "invalid ISR resume changed the wake flag or succeeded");
            in_irq = false;
        }
        in_irq = true;
        check(thread::suspend(target.id) == thread::suspend_result_t::unsupported
                  && thread::resume(target.id) == thread::resume_result_t::unsupported,
              "task API accepted ISR context");
        check(!target.suspended && target.ready.queue, "rejected ISR call changed thread state");
        in_irq = false;
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "kill setup suspension failed");
        check(thread::kill(target.id) == thread::kill_result_t::success, "suspended target could not be killed");
        check(thread::suspend(target.id) == thread::suspend_result_t::invalid_thread
                  && thread::resume(target.id) == thread::resume_result_t::invalid_thread,
              "task API accepted a finished thread");
        in_irq = true;
        check(thread::resume_from_isr(target.id) == thread::resume_result_t::invalid_thread,
              "ISR resume accepted a finished thread");
    }

#if defined(ARTOS_TEST_PRIORITY_AGING)
    void test_suspended_thread_does_not_age()
    {
        setup();
        auto& target = lookup(2);
        check(thread::suspend(target.id) == thread::suspend_result_t::success, "aging setup suspension failed");
        const auto original = target.priority;
        for (unsigned tick = 0; tick < 8; ++tick)
        {
            sched();
        }
        check(target.priority == original && target.ticks_at_priority == 0 && !target.aged_priority,
              "suspended ready thread accumulated priority aging");
    }
#endif

    struct test_case_t
    {
        const char* name;
        void (*run)();
    };

    const std::array cases {
        test_case_t { "idle identity", test_idle_identity },
        test_case_t { "static creation options", test_static_creation_options },
        test_case_t { "dynamic creation options", test_dynamic_creation_options },
        test_case_t { "dynamic creation error", test_dynamic_creation_error },
        test_case_t { "dynamic creation capacity error", test_dynamic_creation_capacity_error },
        test_case_t { "ready queue and bitmap", test_suspend_ready },
        test_case_t { "self suspension", test_self_suspend },
        test_case_t { "self suspension near tick", test_self_suspend_near_tick },
        test_case_t { "resume priorities", test_resume_priorities },
        test_case_t { "resume from idle at low priority", test_resume_idle_at_low_priority },
        test_case_t { "resume before scheduler start", test_resume_before_start },
        test_case_t { "resume preserves pending wait", test_resume_preserves_wait },
        test_case_t { "signal during suspension", test_signal_during_suspension },
        test_case_t { "wake all during suspension", test_wake_all_during_suspension },
        test_case_t { "timeout during suspension", test_timeout_during_suspension },
        test_case_t { "sleep during suspension", test_sleep_during_suspension },
        test_case_t { "mutex ownership", test_mutex_ownership_survives_suspension },
        test_case_t { "ISR resume", test_isr_resume },
        test_case_t { "ISR resume preserves wait", test_isr_resume_blocked },
        test_case_t { "invalid IDs and ISR rejection", test_invalid_and_isr_calls },
#if defined(ARTOS_TEST_PRIORITY_AGING)
        test_case_t { "suspended priority aging", test_suspended_thread_does_not_age },
#endif
    };
}

namespace cpu::port {
    basepri_t irq_save_raise_mask(basepri_t value) noexcept
    {
        const auto previous = interrupt_mask;
        if (interrupt_mask == 0 || value < interrupt_mask)
        {
            interrupt_mask = value;
        }
        return previous;
    }

    void irq_restore_mask(basepri_t previous) noexcept
    {
        interrupt_mask = previous;
        if (previous == 0 && current_thread && current_thread->suspended && on_suspend)
        {
            auto handler = std::exchange(on_suspend, {});
            handler(*static_cast<tcb_t*>(current_thread));
        }
    }

    bool is_in_irq() noexcept
    {
        return in_irq;
    }

    void wait_for_interrupt() noexcept
    {
        std::abort();
    }
}

namespace artos::port {
    stack_word_t* initialize_stack(stack_word_t* top, void (*)(void*), void*) noexcept
    {
        return top;
    }

    void pendsvset() noexcept
    {
        ++pendsv_count;
    }

    std::uint32_t get_cycles_remaining() noexcept
    {
        return cycles_remaining;
    }
}

namespace artos::kernel {
    tick_time_t tick_time() noexcept
    {
        return now;
    }
}

namespace artos::kernel::locked {
    tick_time_t tick_time() noexcept
    {
        return now;
    }
}

int main()
{
    unsigned failures = 0;
    for (const auto& test : cases)
    {
        std::fflush(nullptr);
        const auto child = fork();
        check(child >= 0, "fork failed");
        if (child == 0)
        {
            test.run();
            std::exit(0);
        }
        int status = 0;
        check(waitpid(child, &status, 0) == child, "waitpid failed");
        const bool passed = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        std::printf("%s: %s", passed ? "PASS" : "FAIL", test.name);
        if (WIFSIGNALED(status))
        {
            std::printf(" (signal %d)", WTERMSIG(status));
        }
        std::puts("");
        failures += !passed;
    }
    std::printf("%zu cases, %u failures\n", cases.size(), failures);
    return failures ? 1 : 0;
}
