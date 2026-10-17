#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <limits>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

#include <ucontext.h>

#include <artos.h>
#include <debug_test_suite.h>
#include <wait_queue.h>

// Queue seeding isolates wake policy from the separate public wait/resume tests.
#define private public
#include <event.h>
#undef private

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>

namespace {
    void*    watched_allocation {};
    unsigned watched_deallocations {};
    unsigned unfinished_fibers {};
    unsigned workers_at_deletion {};

    void observe_delete(void* allocation) noexcept
    {
        if (allocation && allocation == watched_allocation)
        {
            ++watched_deallocations;
            workers_at_deletion = unfinished_fibers;
        }
        std::free(allocation);
    }
}

void* operator new(std::size_t size)
{
    if (auto* allocation = std::malloc(size == 0 ? 1 : size))
    {
        return allocation;
    }
    throw std::bad_alloc {};
}

void operator delete(void* allocation) noexcept
{
    observe_delete(allocation);
}

void operator delete(void* allocation, std::size_t) noexcept
{
    observe_delete(allocation);
}

namespace {
    using namespace artos;
    using namespace artos::kernel::internal;
    using namespace artos::thread::internal;
    using host_tests::check;

    std::array<std::optional<tcb_t>, build_config::artos::max_threads> tcbs {};
    std::array<thread::stack_word_t, build_config::artos::max_threads> stacks {};
    tick_time_t                                                        now {};
    cpu::port::basepri_t                                               interrupt_mask {};
    bool                                                               interrupt_context {};
    unsigned                                                           scheduling_requests {};
    unsigned                                                           block_count {};
    bool                                                               unmasked_schedule {};
    std::function<void(tcb_t&)>                                        on_block;
    std::function<void()>                                              on_unmask;

    struct fiber_t
    {
        ucontext_t context {};
        alignas(std::max_align_t) std::array<std::byte, 64 * 1024> stack {};
        std::function<void()>         action;
        std::optional<event_result_t> result;
        bool                          finished {};
    };

    std::array<fiber_t, build_config::artos::max_threads> fibers {};
    bool                                                  fiber_mode {};
    unsigned                                              fiber_switches {};

    void value(event_result_t result, bits_t expected, const char* message)
    {
        check(result.has_value() && *result == expected, message);
    }

    void error(event_result_t result, event_error_t expected, const char* message)
    {
        check(!result.has_value() && result.error() == expected, message);
    }

    tcb_t& install(std::size_t slot, unsigned priority_value)
    {
        auto& thread = tcbs[slot].emplace(
            "event test", static_cast<thread::priority_t>(priority_value), &stacks[slot], &stacks[slot], 1);
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
        assert(thread.ready.queue != nullptr);
        current_thread = &thread;
        thread.state = thread::state_t::running;
    }

    void reset_fixture()
    {
        // No per-case fixture survives the next case in --debug mode. Clear
        // global lists before destroying the TCBs that own their nodes.
        on_block = {};
        on_unmask = {};
        fiber_mode = false;
        fiber_switches = 0;
        unfinished_fibers = 0;
        workers_at_deletion = 0;
        for (auto& fiber : fibers)
        {
            fiber.action = {};
            fiber.result.reset();
            fiber.finished = false;
        }
        for (auto& queue : ready_queue)
        {
            queue.clear();
        }
        block_sleep_queue.clear();
        ready_bitmap.fill(0);
        threads.fill(nullptr);
        current_thread = nullptr;
        next_thread = nullptr;
        thread_count = 0;
        for (auto& thread : tcbs)
        {
            thread.reset();
        }
        now = {};
        interrupt_mask = 0;
        interrupt_context = false;
        scheduling_requests = 0;
        block_count = 0;
        unmasked_schedule = false;
        watched_allocation = nullptr;
        watched_deallocations = 0;
    }

    struct fixture_t
    {
        fixture_t()
        {
            reset_fixture();
            (void)install(0, 15);
            run_as(install(1, 9));
        }

        ~fixture_t()
        {
            reset_fixture();
        }
    };

    void expire(tcb_t& waiter)
    {
        assert(waiter.state == thread::state_t::blocked);
        now = waiter.deadline;
        const critical_section_t cs {};
        sched();
    }

    void seed_waiter(event_t& event, tcb_t& waiter, any_or_all_t requested)
    {
        auto*                    producer = static_cast<tcb_t*>(current_thread);
        const critical_section_t cs {};
        run_as(waiter);
        waiter.waiting_event = &event;
        waiter.waiting_for = requested;
        kernel::locked::block_current(event.waiters, wait_forever);
        run_as(*producer);
    }

    template<bool Replace>
    event_result_t interrupt_update(event_t& event, bits_t bits, bool* woken = nullptr)
    {
        const auto previous = std::exchange(interrupt_context, true);
        const auto result = Replace ? event.set_from_isr(bits, woken) : event.post_from_isr(bits, woken);
        interrupt_context = previous;
        return result;
    }

    void switch_fiber(tcb_t& incoming)
    {
        ++fiber_switches;
        assert(fiber_switches <= 128);
        const auto outgoing_slot = tcb_t::id_layout::extract_id(current_thread->id);
        const auto incoming_slot = tcb_t::id_layout::extract_id(incoming.id);
        run_as(incoming);
        const auto status = swapcontext(&fibers[outgoing_slot].context, &fibers[incoming_slot].context);
        assert(status == 0);
    }

    void fiber_entry(int slot)
    {
        fibers[slot].action();
        fibers[slot].finished = true;
        --unfinished_fibers;
        current_thread->state = thread::state_t::finished;
        {
            const critical_section_t cs;
            sched();
        }
        std::abort(); // A finished test fiber must never resume.
    }

    void start_waiter(
        event_t& event, std::size_t slot, unsigned priority, any_or_all_t requested, bool reset, bool timed)
    {
        fiber_mode = true;
        auto& waiter = install(slot, priority);
        auto& fiber = fibers[slot];
        fiber.action = [&event, slot, requested, reset, timed] {
            if (const auto* any = std::get_if<any_t>(&requested))
            {
                fibers[slot].result
                    = timed ? event.wait_for(*any, reset, tick_t { 100 }) : event.wait_for(*any, reset, wait_forever);
            }
            else
            {
                const auto all = std::get<all_t>(requested);
                fibers[slot].result
                    = timed ? event.wait_for(all, reset, tick_t { 100 }) : event.wait_for(all, reset, wait_forever);
            }
        };
        const auto status = getcontext(&fiber.context);
        assert(status == 0);
        fiber.context.uc_stack.ss_sp = fiber.stack.data();
        fiber.context.uc_stack.ss_size = fiber.stack.size();
        fiber.context.uc_link = nullptr;
        makecontext(&fiber.context, reinterpret_cast<void (*)()>(fiber_entry), 1, static_cast<int>(slot));
        ++unfinished_fibers;
        switch_fiber(waiter);
        assert(!fiber.finished && waiter.waiter.queue != nullptr);
    }

    void finish_ready_workers()
    {
        while (unfinished_fibers != 0)
        {
            bool runnable = false;
            for (std::size_t slot = 2; slot < fibers.size(); ++slot)
            {
                if (fibers[slot].action && !fibers[slot].finished && tcbs[slot]->state == thread::state_t::ready)
                {
                    switch_fiber(*tcbs[slot]);
                    runnable = true;
                    break;
                }
            }
            assert(runnable);
        }
    }

    // Recommended broadcast contract: every matching blocked call receives
    // the notification snapshot, including when another recipient resets bits.
    template<bool FirstReset, bool SecondReset, bool FromISR = false>
    void test_broadcast_snapshot()
    {
        const fixture_t fixture;
        event_t         event;
        start_waiter(event, 2, 1, any_t { 1 }, FirstReset, true);
        start_waiter(event, 3, 2, all_t { 1 }, SecondReset, false);
        if constexpr (FromISR)
        {
            (void)interrupt_update<true>(event, 1);
        }
        else
        {
            (void)event.post(1);
        }
        switch_fiber(*tcbs[2]);
        const bool delivered = fibers[2].finished && fibers[3].finished;
        event.close();
        finish_ready_workers();
        check(delivered, "a matching recipient reblocked after another waiter reset the notification");
        value(*fibers[2].result, 1, "first broadcast snapshot differs");
        value(*fibers[3].result, 1, "second broadcast snapshot differs");
        check(block_sleep_queue.empty(), "broadcast left a deadline registered");
    }

    void test_signaled_snapshot_survives_replacement()
    {
        const fixture_t fixture;
        event_t         event;
        start_waiter(event, 2, 1, any_t { 1 }, false, true);
        (void)event.post(1);
        (void)event.set(0);
        switch_fiber(*tcbs[2]);
        const bool delivered = fibers[2].finished;
        event.close();
        finish_ready_workers();
        check(delivered, "a signaled wait reblocked when producer replaced the bits before resume");
        value(*fibers[2].result, 1, "wait did not retain the notification snapshot");
    }

    template<bool Delete>
    void test_close_multiple_active_waits()
    {
        const fixture_t fixture;
        auto*           event = new event_t;
        watched_allocation = event;
        start_waiter(*event, 2, 1, any_t { 1 }, true, true);
        start_waiter(*event, 3, 2, all_t { 3 }, false, false);
        if constexpr (Delete)
        {
            event->close_and_delete();
            const bool deferred = watched_deallocations == 0;
            finish_ready_workers();
            check(deferred, "close_and_delete destroyed an event with active calls");
            check(watched_deallocations == 1 && workers_at_deletion == 1,
                  "event was not deleted exactly once by its final operation guard");
        }
        else
        {
            event->close_and_wait();
            check(unfinished_fibers == 0, "close_and_wait returned before active calls drained");
            check(event->active_operations == 0, "close_and_wait retained active operations");
            delete event;
        }
        error(*fibers[2].result, event_error_t::closed, "timed wait did not observe close");
        error(*fibers[3].result, event_error_t::closed, "indefinite wait did not observe close");
        check(block_sleep_queue.empty(), "closing event left a deadline registered");
        watched_allocation = nullptr;
    }

    template<int MatchingPriority>
    void test_wake_one_unmatched_head()
    {
        const fixture_t fixture;
        event_t         event { 2 };
        auto&           head = install(2, 1);
        seed_waiter(event, head, any_t { 1 });
        if constexpr (MatchingPriority >= 0)
        {
            auto& matching = install(3, MatchingPriority);
            seed_waiter(event, matching, any_t { 2 });
            (void)kernel::locked::wake_one(event.waiters);
            check(matching.waiter.wake == wake_reason_t::signaled,
                  "unmatched queue head prevented the eligible waiter from waking");
        }
        else
        {
            const bool woke = kernel::locked::wake_one(event.waiters);
            check(!woke, "wake_one reported a wake when no condition matched");
        }
        check(head.waiter.queue == &event.waiters && head.waiter.wake == wake_reason_t::none,
              "wake_one signaled the unmatched queue head");
    }

    template<bool Match>
    void test_wake_one_eligible_fifo()
    {
        const fixture_t fixture;
        event_t         event { 1 };
        auto&           head = install(2, 0);
        auto&           first = install(3, 3);
        auto&           second = install(4, 3);
        seed_waiter(event, head, any_t { 2 });
        seed_waiter(event, first, any_t { Match ? 1U : 2U });
        seed_waiter(event, second, any_t { Match ? 1U : 2U });

        const bool woken = kernel::locked::wake_one(event.waiters);
        check(woken == Match, "wake_one reported the wrong eligible wake");
        check(first.waiter.wake == (Match ? wake_reason_t::signaled : wake_reason_t::none),
              "wake_one did not select the first eligible equal-priority waiter");
        check(head.waiter.queue == &event.waiters && second.waiter.queue == &event.waiters,
              "wake_one removed an unmatched head or a later equal-priority waiter");
        check(head.waiter.wake == wake_reason_t::none && second.waiter.wake == wake_reason_t::none,
              "wake_one signaled more than the selected eligible waiter");
        value(event.wait_for(any_t { 1 }, false, tick_t { 0 }), 1, "wake selection consumed event bits");
    }

    void test_close_between_entry_and_decision()
    {
        const fixture_t fixture;
        event_t         event { 1 };
        on_unmask = [&] { event.close(); };
        error(event.wait_for(any_t { 1 }, true, tick_t { 0 }),
              event_error_t::closed,
              "wait did not recheck closure after entering its operation guard");
        check(event.active_operations == 0, "closed wait leaked its operation guard");
    }

    void test_post_after_atomic_reset()
    {
        const fixture_t fixture;
        event_t         event { 1 };
        on_unmask = [&] { on_unmask = [&] { (void)interrupt_update<false>(event, 1); }; };
        value(event.wait_for(any_t { 1 }, true, tick_t { 0 }), 1, "reset changed the returned snapshot");
        value(event.wait_for(any_t { 1 }, false, tick_t { 0 }),
              1,
              "a new post after atomic reset was consumed by the earlier wait");
    }

    void test_delete_after_final_wait_guard()
    {
        const fixture_t fixture;
        auto*           event = new event_t { 1 };
        watched_allocation = event;
        on_unmask = [&] { on_unmask = [&] { on_unmask = [&] { event->close_and_delete(); }; }; };
        const auto result = event->wait_for(any_t { 1 }, true, tick_t { 0 });
        value(result, 1, "deletion after final guard corrupted the saved result");
        check(watched_deallocations == 1 && !on_unmask,
              "wait did not finish safely across deletion at its final unmask");
        watched_allocation = nullptr;
    }

    template<bool Timed, bool SignalFirst = false>
    void test_killed_waiter_releases_operation()
    {
        const fixture_t fixture;
        auto*           event = new event_t;
        watched_allocation = event;
        start_waiter(*event, 2, 1, any_t { 1 }, false, Timed);
        if constexpr (SignalFirst)
        {
            (void)event->post(1);
        }
        const auto killed = thread::kill(tcbs[2]->id);
        const auto active = event->active_operations;
        const bool detached = tcbs[2]->waiting_event == nullptr;
        event->close_and_delete();
        const auto deletions = watched_deallocations;
        // The killed fiber can never resume. Reclaim a leaked allocation after
        // recording it, so --debug can continue to the remaining cases.
        if (deletions == 0)
        {
            delete event;
        }
        watched_allocation = nullptr;
        unfinished_fibers = 0;
        check(killed == thread::kill_result_t::success, "kill did not accept an event waiter");
        check(active == 0 && deletions == 1,
              "killing an active wait abandoned its guard: close_and_delete leaks and close_and_wait cannot finish");
        check(detached, "killed thread retained an event dependency");
        check(block_sleep_queue.empty(), "killed event waiter retained a deadline");
    }

    void test_matched_event_waiter_is_signaled()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        seed_waiter(event, waiter, any_t { 1 });
        value(event.post(1), 0, "post returned the wrong old state");
        check(waiter.waiter.wake == wake_reason_t::signaled, "matched event waiter was not signaled");
        check(waiter.waiter.queue == nullptr && event.waiters.empty(), "matched waiter remained registered");
        check(waiter.state == thread::state_t::ready, "matched event waiter did not become ready");
        check(waiter.waiting_event == nullptr, "signaled waiter retained its event pointer");
        check(scheduling_requests > 0 && !unmasked_schedule, "thread post failed to schedule under the mask");
    }

    void test_unmatched_event_waiter_remains_registered()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        seed_waiter(event, waiter, any_t { 1 });
        const auto before = scheduling_requests;
        value(event.post(2), 0, "unrelated post returned the wrong old state");
        check(waiter.waiter.queue == &event.waiters, "unmatched waiter lost its queue registration");
        check(waiter.state == thread::state_t::indefinite, "unmatched waiter became ready");
        check(scheduling_requests == before, "unmatched event requested a context switch");
    }

    template<bool Replace>
    void test_isr_wake_flag()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        seed_waiter(event, waiter, any_t { 1 });
        const auto before = scheduling_requests;
        bool       woken = false;
        value(interrupt_update<Replace>(event, 1, &woken), 0, "ISR update returned wrong old state");
        check(woken, "ISR update did not flag a higher-priority waiter");
        check(waiter.waiter.wake == wake_reason_t::signaled, "ISR update did not signal the waiter");
        check(scheduling_requests == before, "ISR update scheduled instead of deferring to its caller");
        check(waiter.waiter.queue == nullptr, "ISR update left the waiter queued");
    }

    template<bool Replace, unsigned Priority, bool Suspended = false>
    void test_isr_no_preemption()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, Priority);
        seed_waiter(event, waiter, any_t { 1 });
        waiter.suspended = Suspended;
        bool       woken = false;
        const auto before = scheduling_requests;
        value(interrupt_update<Replace>(event, 1, &woken), 0, "ISR update returned wrong old state");
        check(!woken, "ISR update incorrectly requested preemption");
        check(waiter.state == thread::state_t::ready, "matching waiter did not become ready");
        check(scheduling_requests == before, "ISR update scheduled internally");
        if constexpr (Suspended)
        {
            check(waiter.ready.queue == nullptr, "suspended waiter was made runnable");
        }
    }

    template<bool Replace>
    void test_isr_null_and_accumulated_flags()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        seed_waiter(event, waiter, any_t { 1 });
        value(interrupt_update<Replace>(event, 1, nullptr), 0, "nullptr ISR update failed");
        check(waiter.waiter.wake == wake_reason_t::signaled, "nullptr ISR call did not wake the waiter");

        // The seeded waiter is ready but has not resumed to perform its reset.
        // Check accumulation independently, on an empty event with no waiters.
        event_t idle;
        bool    woken = true;
        value(interrupt_update<Replace>(idle, 0, &woken), 0, "empty ISR update failed");
        check(woken, "ISR update cleared a flag accumulated by an earlier operation");
    }

    template<bool Replace>
    void test_nonreset_waiter_preserves_event()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        seed_waiter(event, waiter, any_t { 1 });
        bool woken = false;
        (void)interrupt_update<Replace>(event, 1, &woken);
        check(waiter.waiter.wake == wake_reason_t::signaled, "non-resetting waiter was not signaled");
        check(event.bitset == 1, "wake path cleared bits even though the waiter requested reset=false");
    }

    void test_nonreset_broadcast_wakes_both()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           first = install(2, 1);
        auto&           second = install(3, 2);
        seed_waiter(event, first, any_t { 1 });
        seed_waiter(event, second, all_t { 1 });
        (void)event.post(1);
        check(first.waiter.wake == wake_reason_t::signaled && second.waiter.wake == wake_reason_t::signaled,
              "posting one bit did not wake both matching non-resetting waiters");
    }

    template<bool Replace>
    void test_broadcast_sets_highest_wake_flag()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           low = install(2, 12);
        auto&           high = install(3, 1);
        seed_waiter(event, low, any_t { 1 });
        seed_waiter(event, high, any_t { 1 });
        bool woken = false;
        (void)interrupt_update<Replace>(event, 1, &woken);
        check(woken && high.waiter.wake == wake_reason_t::signaled,
              "lower-priority first waiter prevented a later high-priority wake/ISR flag");
    }

    void test_partial_all_wait_keeps_registration()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        seed_waiter(event, waiter, all_t { 3 });
        (void)event.post(1);
        check(waiter.waiter.queue == &event.waiters, "partial all-match removed the waiter");
        (void)event.post(2);
        check(waiter.waiter.wake == wake_reason_t::signaled, "completed all-match did not wake waiter");
    }

    void test_wake_one_skips_unmatched_first_waiter()
    {
        const fixture_t fixture;
        event_t         event { 2 };
        auto&           first = install(2, 12);
        auto&           matching = install(3, 1);
        seed_waiter(event, first, any_t { 1 });
        seed_waiter(event, matching, any_t { 2 });
        (void)kernel::locked::wake_one(event.waiters);
        check(matching.waiter.wake == wake_reason_t::signaled && first.waiter.wake == wake_reason_t::none,
              "wake_one tested the selected thread instead of the matching higher-priority candidate");
    }

    void test_wake_one_does_not_select_unmatched_candidate()
    {
        const fixture_t fixture;
        event_t         event { 1 };
        auto&           matching = install(2, 12);
        auto&           unmatched = install(3, 1);
        seed_waiter(event, matching, any_t { 1 });
        seed_waiter(event, unmatched, any_t { 2 });
        (void)kernel::locked::wake_one(event.waiters);
        check(matching.waiter.wake == wake_reason_t::signaled && unmatched.waiter.wake == wake_reason_t::none,
              "wake_one signaled a candidate whose event condition was not satisfied");
    }

    template<typename Event = event_t>
    void test_copy_does_not_create_malformed_queue()
    {
        const fixture_t fixture;
        if constexpr (std::is_copy_constructible_v<Event>)
        {
            const Event original;
            Event       copy { original };
            // Inspect only: iterating/posting to this copied sentinel would
            // interpret the original sentinel as a wait_queue_node_t.
            check(copy.waiters.empty() && copy.close_queue.empty(),
                  "copying an empty event creates malformed, apparently nonempty wait queues");
        }
    }

    template<bool FromISR, bool Replace, bool Reset, bool All = false>
    void test_public_wait_survives_signal()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        event_result_t  update_result = std::unexpected(event_error_t::closed);
        on_block = [&](tcb_t&) {
            if constexpr (FromISR)
            {
                update_result = interrupt_update<Replace>(event, 3);
            }
            else
            {
                update_result = Replace ? event.set(3) : event.post(3);
            }
        };
        run_as(waiter);
        const auto result
            = All ? event.wait_for(all_t { 3 }, Reset, tick_t { 5 }) : event.wait_for(any_t { 1 }, Reset, tick_t { 5 });
        value(update_result, 0, "producer update failed");
        value(result, 3, "signaled wait lost the delivered event/snapshot before resuming");
        check(block_count == 1, "signaled wait blocked again instead of returning its delivery");
    }

    void test_timeout_clears_event_dependency()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        run_as(waiter);
        error(event.wait_for(any_t { 1 }, false, tick_t { 5 }),
              event_error_t::timeout,
              "unposted event did not time out");
        check(waiter.waiter.queue == nullptr && waiter.block_sleep.queue == nullptr,
              "timeout retained queue/deadline registration");
        check(waiter.waiting_event == nullptr, "timeout left a stale waiting_event pointer in the TCB");
    }

    void test_timeout_does_not_filter_next_wait()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        run_as(waiter);
        error(event.wait_for(any_t { 1 }, false, tick_t { 5 }),
              event_error_t::timeout,
              "initial event wait did not time out");
        wait_queue_t unrelated;
        on_block = [&](tcb_t&) { (void)kernel::locked::wake_all(unrelated); };
        wake_reason_t reason;
        {
            critical_section_t cs;
            reason = kernel::locked::block_current(unrelated, now + 5, std::move(cs));
        }
        check(reason == wake_reason_t::signaled,
              "stale event dependency prevented a subsequent unrelated queue from signaling");
    }

    void test_timeout_across_rollover()
    {
        const fixture_t fixture;
        now.ticks = std::numeric_limits<tick_t>::max() - 2;
        event_t event;
        auto&   waiter = install(2, 1);
        run_as(waiter);
        error(event.wait_for(all_t { 3 }, false, tick_t { 5 }),
              event_error_t::timeout,
              "event timeout failed across tick rollover");
        check(now.generation == 1 && now.ticks == 2, "event used an incorrect rollover deadline");
    }

    void test_close_cancels_active_wait()
    {
        const fixture_t fixture;
        event_t         event;
        auto&           waiter = install(2, 1);
        on_block = [&](tcb_t&) { event.close(); };
        run_as(waiter);
        error(event.wait_for(any_t { 1 }, false, wait_forever),
              event_error_t::closed,
              "close did not cancel the active wait");
        check(waiter.waiting_event == nullptr && waiter.waiter.queue == nullptr,
              "close left event dependency or queue registration");
        check(event.active_operations == 0, "cancelled wait leaked its operation guard");
        event.close_and_wait();
    }

    void test_close_and_delete_idle()
    {
        const fixture_t fixture;
        auto*           event = new event_t;
        watched_allocation = event;
        event->close_and_delete();
        check(watched_deallocations == 1, "idle close_and_delete did not delete exactly once");
        watched_allocation = nullptr;
    }

    void test_close_and_delete_defers_for_active_wait()
    {
        const fixture_t fixture;
        auto*           event = new event_t;
        watched_allocation = event;
        auto& waiter = install(2, 1);
        bool  deleted_before_wait_exit = false;
        on_block = [&](tcb_t&) {
            event->close_and_delete();
            deleted_before_wait_exit = watched_deallocations != 0;
        };
        run_as(waiter);
        const auto result = event->wait_for(any_t { 1 }, false, wait_forever);
        error(result, event_error_t::closed, "deleting event did not cancel active wait");
        check(!deleted_before_wait_exit, "event was deleted while a wait still held its operation guard");
        check(watched_deallocations == 1, "final wait did not delete closed event exactly once");
        check(waiter.waiting_event == nullptr, "deleted event was still referenced by its waiter");
        watched_allocation = nullptr;
    }
}

namespace cpu::port {
    basepri_t irq_save_raise_mask(basepri_t priority) noexcept
    {
        const auto previous = interrupt_mask;
        if (interrupt_mask == 0 || priority < interrupt_mask)
        {
            interrupt_mask = priority;
        }
        return previous;
    }

    void irq_restore_mask(basepri_t previous) noexcept
    {
        interrupt_mask = previous;
        if (interrupt_mask == 0 && !interrupt_context && on_unmask)
        {
            auto handler = std::exchange(on_unmask, {});
            handler();
        }
        if (interrupt_mask != 0 || interrupt_context || !current_thread
            || (current_thread->state != artos::thread::state_t::blocked
                && current_thread->state != artos::thread::state_t::indefinite
                && !(fiber_mode && current_thread->state == artos::thread::state_t::finished)))
        {
            return;
        }
        if (fiber_mode)
        {
            assert(next_thread && next_thread != current_thread);
            switch_fiber(*static_cast<tcb_t*>(next_thread));
            return;
        }
        // Only the CPU port is simulated. Real blocking and deadline code run
        // above this boundary. An unexpected retry expires instead of hanging.
        ++block_count;
        auto& waiter = *static_cast<tcb_t*>(current_thread);
        assert(next_thread && next_thread != &waiter);
        run_as(*static_cast<tcb_t*>(next_thread));
        if (on_block)
        {
            auto handler = std::exchange(on_block, {});
            handler(waiter);
        }
        if (waiter.waiter.queue)
        {
            if (waiter.state == artos::thread::state_t::blocked)
            {
                expire(waiter);
            }
            else
            {
                const artos::critical_section_t cs;
                (void)artos::kernel::locked::wake_all(*waiter.waiter.queue, artos::wake_reason_t::cancelled);
                sched();
            }
        }
        assert(waiter.waiter.queue == nullptr);
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
        unmasked_schedule |= interrupt_mask == 0;
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

namespace board::stopwatch {
    std::uint32_t count() noexcept
    {
        return 0;
    }
}

int main(int argc, char** argv)
{
    const std::array cases {
        host_tests::test_case_t { "wake_one preserves eligible FIFO", test_wake_one_eligible_fifo<true> },
        host_tests::test_case_t { "wake_one preserves all-unmatched queue", test_wake_one_eligible_fifo<false> },
        host_tests::test_case_t { "kill timed event waiter releases operation",
                                  test_killed_waiter_releases_operation<true> },
        host_tests::test_case_t { "kill indefinite event waiter releases operation",
                                  test_killed_waiter_releases_operation<false> },
        host_tests::test_case_t { "kill signaled event waiter releases operation",
                                  test_killed_waiter_releases_operation<true, true> },
        host_tests::test_case_t { "close between operation entry and decision", test_close_between_entry_and_decision },
        host_tests::test_case_t { "new post after atomic reset", test_post_after_atomic_reset },
        host_tests::test_case_t { "delete after final wait guard", test_delete_after_final_wait_guard },
        host_tests::test_case_t { "recommended broadcast snapshot both reset", test_broadcast_snapshot<true, true> },
        host_tests::test_case_t { "recommended broadcast snapshot reset then nonreset",
                                  test_broadcast_snapshot<true, false> },
        host_tests::test_case_t { "recommended broadcast snapshot nonreset then reset",
                                  test_broadcast_snapshot<false, true> },
        host_tests::test_case_t { "recommended broadcast snapshot neither reset",
                                  test_broadcast_snapshot<false, false> },
        host_tests::test_case_t { "recommended ISR broadcast snapshot both reset",
                                  test_broadcast_snapshot<true, true, true> },
        host_tests::test_case_t { "recommended signaled snapshot survives replacement",
                                  test_signaled_snapshot_survives_replacement },
        host_tests::test_case_t { "close_and_wait drains mixed active waits", test_close_multiple_active_waits<false> },
        host_tests::test_case_t { "close_and_delete drains mixed active waits",
                                  test_close_multiple_active_waits<true> },
        host_tests::test_case_t { "wake_one unmatched sole waiter", test_wake_one_unmatched_head<-1> },
        host_tests::test_case_t { "wake_one unmatched higher-priority head", test_wake_one_unmatched_head<12> },
        host_tests::test_case_t { "wake_one unmatched equal-priority head", test_wake_one_unmatched_head<1> },
        host_tests::test_case_t { "matched waiter", test_matched_event_waiter_is_signaled },
        host_tests::test_case_t { "unmatched waiter", test_unmatched_event_waiter_remains_registered },
        host_tests::test_case_t { "ISR post wakes high-priority waiter without scheduling", test_isr_wake_flag<false> },
        host_tests::test_case_t { "ISR set wakes high-priority waiter without scheduling", test_isr_wake_flag<true> },
        host_tests::test_case_t { "ISR post lower-priority waiter", test_isr_no_preemption<false, 12> },
        host_tests::test_case_t { "ISR set equal-priority waiter", test_isr_no_preemption<true, 9> },
        host_tests::test_case_t { "ISR post suspended waiter", test_isr_no_preemption<false, 1, true> },
        host_tests::test_case_t { "ISR set suspended waiter", test_isr_no_preemption<true, 1, true> },
        host_tests::test_case_t { "ISR post nullptr and accumulated flag", test_isr_null_and_accumulated_flags<false> },
        host_tests::test_case_t { "ISR set nullptr and accumulated flag", test_isr_null_and_accumulated_flags<true> },
        host_tests::test_case_t { "ISR post preserves bits before resume",
                                  test_nonreset_waiter_preserves_event<false> },
        host_tests::test_case_t { "ISR set preserves bits before resume", test_nonreset_waiter_preserves_event<true> },
        host_tests::test_case_t { "broadcast makes both waiters ready", test_nonreset_broadcast_wakes_both },
        host_tests::test_case_t { "ISR post broadcast reports highest-priority wake",
                                  test_broadcast_sets_highest_wake_flag<false> },
        host_tests::test_case_t { "ISR set broadcast reports highest-priority wake",
                                  test_broadcast_sets_highest_wake_flag<true> },
        host_tests::test_case_t { "partial all-wait preserves queue", test_partial_all_wait_keeps_registration },
        host_tests::test_case_t { "wake_one skips unmatched first waiter", test_wake_one_skips_unmatched_first_waiter },
        host_tests::test_case_t { "wake_one rejects unmatched candidate",
                                  test_wake_one_does_not_select_unmatched_candidate },
        host_tests::test_case_t { "copy does not create malformed queues",
                                  test_copy_does_not_create_malformed_queue<> },
        host_tests::test_case_t { "thread post delivery survives resume",
                                  test_public_wait_survives_signal<false, false, false> },
        host_tests::test_case_t { "thread set reset delivery survives resume",
                                  test_public_wait_survives_signal<false, true, true> },
        host_tests::test_case_t { "ISR post delivery survives resume",
                                  test_public_wait_survives_signal<true, false, false> },
        host_tests::test_case_t { "ISR post all-reset delivery survives resume",
                                  test_public_wait_survives_signal<true, false, true, true> },
        host_tests::test_case_t { "ISR set reset delivery survives resume",
                                  test_public_wait_survives_signal<true, true, true> },
        host_tests::test_case_t { "ISR set all-delivery survives resume",
                                  test_public_wait_survives_signal<true, true, false, true> },
        host_tests::test_case_t { "timeout clears event dependency", test_timeout_clears_event_dependency },
        host_tests::test_case_t { "timeout does not filter next object wait", test_timeout_does_not_filter_next_wait },
        host_tests::test_case_t { "timeout across rollover", test_timeout_across_rollover },
        host_tests::test_case_t { "close cancels active wait", test_close_cancels_active_wait },
        host_tests::test_case_t { "close_and_delete idle", test_close_and_delete_idle },
        host_tests::test_case_t { "close_and_delete defers for active wait",
                                  test_close_and_delete_defers_for_active_wait },
    };
    return host_tests::run_cases(argc, argv, cases);
}
