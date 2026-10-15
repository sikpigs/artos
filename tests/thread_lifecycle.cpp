#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <utility>

#include <artos.h>
#include <port/artos_port.h>
#include <port/cpu_port.h>

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
        auto& thread = tcbs[slot].emplace("lifecycle", priority(priority_value), &stacks[slot], &stacks[slot], 1);
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
        current_thread = &thread;
        thread.state = thread::state_t::running;
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
        for (auto& tcb : tcbs)
        {
            tcb.reset();
        }
        thread_count = 0;
        current_thread = nullptr;
        next_thread = nullptr;
        now = {};
        interrupt_mask = 0;
        on_block = {};

        (void)install(0, 15);
        run_as(install(1, 9));
        (void)install(2, 5);
    }

    void test_kill_ready_thread()
    {
        setup();
        auto& target = *tcbs[2];
        check(thread::kill(target.id) == thread::kill_result_t::success, "kill rejected a valid thread");
        check(target.state == thread::state_t::finished, "kill did not finish target");
        check(target.ready.queue == nullptr, "kill left target in a ready queue");
        check(thread::kill(target.id) == thread::kill_result_t::success, "repeat kill was not harmless");
    }

    void test_kill_blocked_thread()
    {
        setup();
        auto&        target = *tcbs[2];
        wait_queue_t waiters {};
        waiters.push_back(target.waiter);
        target.state = thread::state_t::indefinite;
        check(thread::kill(target.id) == thread::kill_result_t::success, "kill rejected a blocked thread");
        check(target.state == thread::state_t::finished, "kill did not finish blocked target");
        check(target.waiter.queue == nullptr && waiters.empty(), "kill left target in its wait queue");
        check(target.waiter.wake == wake_reason_t::cancelled, "kill did not cancel target's wait");
    }

    void test_invalid_thread_ids()
    {
        setup();
        check(thread::kill(thread::invalid_id) == thread::kill_result_t::invalid_thread, "kill accepted an invalid ID");
        check(thread::join(thread::invalid_id, tick_t { 0 }) == thread::join_result_t::invalid_thread,
              "join accepted an invalid ID");
        check(thread::kill(tcbs[0]->id) == thread::kill_result_t::invalid_thread, "kill accepted the idle thread");
    }

    void test_join_finished_thread()
    {
        setup();
        auto& target = *tcbs[2];
        target.state = thread::state_t::finished;
        target.ready.queue->remove(target.ready);
        check(thread::join(target.id, wait_forever) == thread::join_result_t::success,
              "join blocked on an already finished thread");
    }

    void test_join_self_detects_deadlock()
    {
        setup();
        check(thread::join(current_thread->id, wait_forever) == thread::join_result_t::deadlock_detected,
              "join did not reject joining the current thread");
    }

    void test_join_woken_by_kill()
    {
        setup();
        auto& waiter = *tcbs[1];
        auto& target = *tcbs[2];
        on_block = [&](tcb_t& blocked) {
            check(&blocked == &waiter, "wrong thread entered join wait");
            check(thread::kill(target.id) == thread::kill_result_t::success, "kill failed while joiner was blocked");
            check(waiter.waiter.wake == wake_reason_t::signaled, "kill did not wake joiner");
            check(waiter.waiter.queue == nullptr, "kill left joiner registered");
            run_as(waiter);
        };
        check(thread::join(target.id, wait_forever) == thread::join_result_t::success,
              "join did not complete after target was killed");
    }

    void test_join_timeout()
    {
        setup();
        auto& waiter = *tcbs[1];
        auto& target = *tcbs[2];
        on_block = [&](tcb_t& blocked) {
            check(&blocked == &waiter, "wrong thread entered timed join wait");
            now = blocked.deadline;
            {
                const critical_section_t cs {};
                sched();
            }
            check(waiter.waiter.wake == wake_reason_t::timeout, "timed join did not time out");
            check(waiter.waiter.queue == nullptr, "timed join timeout left joiner registered");
            check(waiter.block_sleep.queue == nullptr, "timed join timeout left deadline registered");
            run_as(waiter);
        };
        check(thread::join(target.id, tick_t { 4 }) == thread::join_result_t::timeout,
              "timed join returned wrong result");
    }

    void test_join_zero_timeout()
    {
        setup();
        auto& target = *tcbs[2];
        check(thread::join(target.id, tick_t { 0 }) == thread::join_result_t::timeout,
              "zero-time join did not time out immediately");
        check(target.joiners.empty(), "zero-time join registered a waiter");
    }

    void test_join_cycle_detected()
    {
        setup();
        auto& first = *tcbs[1];
        auto& second = *tcbs[2];
        on_block = [&](tcb_t& blocked) {
            check(&blocked == &first, "wrong first thread entered join wait");
            run_as(second);
            check(thread::join(first.id, wait_forever) == thread::join_result_t::deadlock_detected,
                  "join did not detect a two-thread wait cycle");
            check(thread::kill(first.id) == thread::kill_result_t::success, "could not clean up first joiner");
            run_as(first);
        };
        check(thread::join(second.id, wait_forever) == thread::join_result_t::success,
              "join did not resume after its waiter was killed");
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
        if (interrupt_mask == 0 && current_thread
            && (current_thread->state == artos::thread::state_t::blocked
                || current_thread->state == artos::thread::state_t::indefinite))
        {
            auto handler = std::exchange(on_block, {});
            check(static_cast<bool>(handler), "unexpected blocking call");
            auto& waiter = *static_cast<tcb_t*>(current_thread);
            run_as(*static_cast<tcb_t*>(next_thread));
            handler(waiter);
            check(waiter.waiter.queue == nullptr, "waiter resumed before it was removed from its queue");
            run_as(waiter);
        }
    }

    bool is_in_irq() noexcept
    {
        return false;
    }

    void wait_for_interrupt() noexcept
    {
        std::abort();
    }
}

namespace artos::port {
    stack_word_t* initialize_stack(stack_word_t* stack_top, void (*)(void*), void*) noexcept
    {
        return stack_top;
    }

    void pendsvset() noexcept
    {
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
}

namespace artos::kernel::locked {
    tick_time_t tick_time() noexcept
    {
        return now;
    }
}

int main()
{
    test_kill_ready_thread();
    test_kill_blocked_thread();
    test_invalid_thread_ids();
    test_join_finished_thread();
    test_join_self_detects_deadlock();
    test_join_woken_by_kill();
    test_join_timeout();
    test_join_zero_timeout();
    test_join_cycle_detected();
    std::puts("PASS: thread lifecycle");
}
