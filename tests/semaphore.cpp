#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <new>
#include <optional>
#include <string_view>
#include <utility>

#include <sys/resource.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#include <artos.h>
#include <semaphore.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>

namespace {
    void*    watched_allocation {};
    unsigned watched_deallocations {};
    unsigned unfinished_workers {};
    bool     deleting_call_active {};
    bool     deferred_delete_expected {};

    void observe_delete(void* allocation) noexcept
    {
        if (allocation && allocation == watched_allocation)
        {
            ++watched_deallocations;
            // Deferred deletion runs in the final acquisition's guard
            // destructor, before that worker returns from acquire().
            const auto expected_workers = deferred_delete_expected ? 1U : 0U;
            if (unfinished_workers != expected_workers || watched_deallocations != 1)
            {
                std::fprintf(stderr, "FAIL: semaphore deleted at the wrong operation boundary, or deleted twice\n");
                std::exit(1);
            }
            // Quarantine this allocation until the API has returned. The port
            // detects continued kernel access after delete without reading freed
            // storage or relying on an allocator's reuse of the same address.
            return;
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

    std::array<std::optional<tcb_t>, build_config::artos::max_threads> tcbs {};
    std::array<thread::stack_word_t, build_config::artos::max_threads> stacks {};
    tick_time_t                                                        now {};
    cpu::port::basepri_t                                               interrupt_mask {};
    bool                                                               interrupt_context = false;
    unsigned                                                           scheduling_requests = 0;
    unsigned                                                           block_count = 0;
    bool                                                               require_masked_schedule = false;
    std::function<void(tcb_t&)>                                        on_block;
    std::function<void()>                                              on_unmask;

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
        auto& thread = tcbs[slot].emplace("semaphore test", priority(priority_value), &stacks[slot], &stacks[slot], 1);
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

    semaphore_result_t release_from_interrupt(semaphore_t& semaphore, bool* woken)
    {
        const auto previous = std::exchange(interrupt_context, true);
        const auto result = semaphore.release_from_isr(woken);
        interrupt_context = previous;
        return result;
    }

    void test_constructed_open()
    {
        semaphore_t semaphore { 2, 0 };
        check(semaphore.try_acquire() == semaphore_result_t::success, "new semaphore is closed rather than available");
    }

    void test_initial_permits_in_use()
    {
        semaphore_t semaphore { 2, 1 };
        check(semaphore.try_acquire() == semaphore_result_t::success, "remaining permit was not available");
        check(semaphore.try_acquire() == semaphore_result_t::full, "full semaphore allowed acquisition");
        check(semaphore.release() == semaphore_result_t::success, "first permit release failed");
        check(semaphore.release() == semaphore_result_t::success, "second permit release failed");
        check(semaphore.try_acquire() == semaphore_result_t::success, "first released permit was not available");
        check(semaphore.try_acquire() == semaphore_result_t::success, "second released permit was not available");
        check(semaphore.try_acquire() == semaphore_result_t::full, "release supplied an extra permit");
    }

    void test_release_frees_permit()
    {
        semaphore_t semaphore { 2, 2 };
        check(semaphore.release() == semaphore_result_t::success, "release from full semaphore failed");
        check(semaphore.try_acquire() == semaphore_result_t::success, "release did not free a permit");
        check(semaphore.try_acquire() == semaphore_result_t::full, "release freed more than one permit");
    }

    void test_release_when_unused()
    {
        semaphore_t semaphore { 1, 0 };
        bool        woken = false;
        check(release_from_interrupt(semaphore, &woken) == semaphore_result_t::success,
              "unused release was not a successful no-op");
        check(!woken, "unused release changed wake flag");
        check(semaphore.try_acquire() == semaphore_result_t::success, "unused release damaged capacity");
        check(semaphore.try_acquire() == semaphore_result_t::full, "unused release added an extra permit");
    }

    void test_zero_wait()
    {
        semaphore_t semaphore { 1, 0 };
        check(semaphore.acquire(0) == semaphore_result_t::success, "zero-wait acquisition did not take a permit");
        check(semaphore.acquire(0) == semaphore_result_t::timeout, "zero-wait acquisition of full semaphore succeeded");
        check(block_count == 0, "zero-wait acquisition blocked");
    }

    void test_timed_wake()
    {
        semaphore_t semaphore { 1, 1 };
        auto&       waiter = install(2, 1);
        on_block = [&](tcb_t& blocked) {
            check(blocked.deadline == now + 5, "wrong acquisition deadline");
            const auto before = scheduling_requests;
            check(semaphore.release() == semaphore_result_t::success, "thread release failed");
            check(scheduling_requests > before, "thread release did not request scheduling");
            check(blocked.waiter.wake == wake_reason_t::signaled, "release did not signal waiter");
            check(blocked.block_sleep.queue == nullptr, "release retained acquisition deadline");
        };
        run_as(waiter);
        check(semaphore.acquire(5) == semaphore_result_t::success, "signaled timed waiter did not acquire");
        check(semaphore.try_acquire() == semaphore_result_t::full, "resumed waiter did not take a permit");
    }

    void test_indefinite_isr_wake()
    {
        semaphore_t semaphore { 1, 1 };
        auto&       waiter = install(2, 1);
        on_block = [&](tcb_t& blocked) {
            check(blocked.state == thread::state_t::indefinite, "indefinite acquisition used a timed state");
            check(blocked.block_sleep.queue == nullptr, "indefinite acquisition registered a deadline");
            bool       woken = false;
            const auto before = scheduling_requests;
            interrupt_context = true;
            check(release_from_interrupt(semaphore, &woken) == semaphore_result_t::success, "ISR release failed");
            interrupt_context = false;
            check(woken, "ISR release did not report higher-priority waiter");
            check(scheduling_requests == before, "deferred ISR release scheduled internally");
        };
        run_as(waiter);
        check(semaphore.acquire(wait_forever) == semaphore_result_t::success, "ISR-signaled waiter did not acquire");
        check(semaphore.try_acquire() == semaphore_result_t::full, "ISR release supplied an extra permit");
    }

    void test_wake_flag_accumulates()
    {
        semaphore_t semaphore { 2, 0 };
        bool        woken = true;
        check(release_from_interrupt(semaphore, &woken) == semaphore_result_t::success, "deferred release failed");
        check(woken, "release cleared an existing wake indication");
        woken = false;
        const auto before = scheduling_requests;
        check(release_from_interrupt(semaphore, &woken) == semaphore_result_t::success,
              "second deferred release failed");
        check(!woken, "release without waiters reported a wake");
        check(scheduling_requests == before, "release without waiters scheduled");
    }

    void test_null_wake_pointer()
    {
        semaphore_t semaphore { 1, 1 };
        auto&       waiter = install(2, 1);
        on_block = [&](tcb_t&) {
            const auto before = scheduling_requests;
            check(release_from_interrupt(semaphore, nullptr) == semaphore_result_t::success,
                  "null-pointer release failed");
            check(scheduling_requests == before, "null-pointer deferred release scheduled internally");
        };
        run_as(waiter);
        check(semaphore.acquire(wait_forever) == semaphore_result_t::success,
              "null-pointer release did not wake waiter");
    }

    void test_isr_try_acquire()
    {
        semaphore_t semaphore { 1, 0 };
        interrupt_context = true;
        check(semaphore.try_acquire() == semaphore_result_t::success, "ISR could not take a permit");
        check(semaphore.try_acquire() == semaphore_result_t::full, "ISR acquired full semaphore");
        interrupt_context = false;
        check(block_count == 0, "ISR try_acquire blocked");
    }

    void test_isr_blocking_acquire_rejected()
    {
        semaphore_t semaphore { 1, 0 };
        interrupt_context = true;
        check(semaphore.acquire(0) == semaphore_result_t::unsupported, "blocking API accepted ISR context");
        check(semaphore.acquire(wait_forever) == semaphore_result_t::unsupported, "forever API accepted ISR context");
        interrupt_context = false;
        check(block_count == 0, "ISR acquisition attempted to block");
        check(semaphore.try_acquire() == semaphore_result_t::success, "rejected ISR acquisition consumed permit");
    }

    void test_timeout()
    {
        semaphore_t semaphore { 1, 1 };
        auto&       waiter = install(2, 12);
        on_block = [&](tcb_t& blocked) { expire(blocked); };
        run_as(waiter);
        check(semaphore.acquire(5) == semaphore_result_t::timeout, "full semaphore did not time out");
        check(semaphore.try_acquire() == semaphore_result_t::full, "timeout created a permit");
        check(semaphore.release() == semaphore_result_t::success, "release after timeout failed");
        check(semaphore.try_acquire() == semaphore_result_t::success, "timeout damaged semaphore count");
    }

    void test_isr_full_acquire_rejected()
    {
        semaphore_t semaphore { 1, 1 };
        interrupt_context = true;
        on_block = [](tcb_t&) { check(false, "ISR acquisition attempted to block"); };
        check(semaphore.acquire(5) == semaphore_result_t::unsupported, "full acquisition accepted ISR context");
        interrupt_context = false;
        check(block_count == 0, "ISR acquisition blocked");
    }

    void test_maximum_count_does_not_wrap()
    {
        constexpr auto maximum = std::numeric_limits<semaphore_t::count_t>::max();
        semaphore_t    semaphore { maximum, maximum - 1 };
        check(semaphore.try_acquire() == semaphore_result_t::success, "last permit could not be acquired");
        check(semaphore.try_acquire() == semaphore_result_t::full, "maximum occupancy overflowed");
        check(semaphore.release() == semaphore_result_t::success, "release from maximum occupancy failed");
        check(semaphore.try_acquire() == semaphore_result_t::success, "released last permit could not be reacquired");
    }

    void test_waiter_does_not_donate_priority()
    {
        semaphore_t semaphore { 1, 1 };
        auto&       releasing_thread = *tcbs[1];
        auto&       waiter = install(2, 1);
        on_block = [&](tcb_t&) {
            check(releasing_thread.priority == releasing_thread.base_priority, "semaphore waiter donated priority");
            check(waiter.waiting_mutex == nullptr, "semaphore wait registered a mutex dependency");
            run_as(releasing_thread);
            check(semaphore.release() == semaphore_result_t::success, "permit release failed");
        };
        run_as(waiter);
        check(semaphore.acquire(wait_forever) == semaphore_result_t::success, "waiter did not acquire");
    }

    void test_deadline_rollover()
    {
        semaphore_t semaphore { 1, 1 };
        now = { .ticks = std::numeric_limits<tick_t>::max() - 2, .generation = 4 };
        const auto expected = now + 5;
        auto&      waiter = install(2, 12);
        on_block = [&](tcb_t& blocked) {
            check(blocked.deadline == expected, "acquisition deadline lost rollover generation");
            expire(blocked);
        };
        run_as(waiter);
        check(semaphore.acquire(5) == semaphore_result_t::timeout, "rollover deadline did not expire");
    }

    void test_retry_keeps_deadline()
    {
        semaphore_t semaphore { 1, 1 };
        auto&       waiter = install(2, 12);
        const auto  expected = now + 5;
        on_block = [&](tcb_t&) {
            check(release_from_interrupt(semaphore, nullptr) == semaphore_result_t::success,
                  "retry setup release failed");
            check(semaphore.try_acquire() == semaphore_result_t::success, "barger could not consume released permit");
            now = now + 2;
            on_block = [&](tcb_t& blocked) {
                check(blocked.deadline == expected, "retry extended acquisition deadline");
                expire(blocked);
            };
        };
        run_as(waiter);
        check(semaphore.acquire(5) == semaphore_result_t::timeout, "barged waiter did not time out");
        check(block_count == 2, "test did not exercise reacquisition retry");
    }

    void test_close_rejects_operations()
    {
        semaphore_t semaphore { 2, 1 };
        semaphore.close();
        semaphore.close();
        bool woken = true;
        check(semaphore.try_acquire() == semaphore_result_t::closed, "closed semaphore accepted try_acquire");
        check(semaphore.acquire(0) == semaphore_result_t::closed, "closed semaphore accepted timed acquire");
        check(semaphore.acquire(wait_forever) == semaphore_result_t::closed,
              "closed semaphore accepted forever acquire");
        check(semaphore.release() == semaphore_result_t::closed, "closed semaphore accepted release");
        check(release_from_interrupt(semaphore, &woken) == semaphore_result_t::closed,
              "closed semaphore accepted deferred release");
        check(woken, "closed release cleared wake flag");
        semaphore.close_and_wait();
        check(block_count == 0, "idle close_and_wait blocked");
    }

    void test_close_cancels_waiters()
    {
        semaphore_t semaphore { 1, 1 };
        auto&       first = install(2, 3);
        on_block = [&](tcb_t& first_blocked) {
            auto& second = install(3, 1);
            on_block = [&](tcb_t& second_blocked) {
                semaphore.close();
                check(first_blocked.waiter.wake == wake_reason_t::cancelled, "close did not cancel timed waiter");
                check(second_blocked.waiter.wake == wake_reason_t::cancelled, "close did not cancel indefinite waiter");
                check(first_blocked.block_sleep.queue == nullptr, "close retained timed deadline");
            };
            run_as(second);
            check(semaphore.acquire(wait_forever) == semaphore_result_t::closed, "indefinite waiter ignored close");
        };
        run_as(first);
        check(semaphore.acquire(5) == semaphore_result_t::closed, "timed waiter ignored close");
        semaphore.close_and_wait();
    }

    void test_close_between_entry_and_acquisition()
    {
        semaphore_t semaphore { 2, 1 };
        auto&       acquiring_thread = *tcbs[1];
        // Simulate another thread closing at the operation guard's unmask,
        // before acquire enters its count/state decision section.
        on_unmask = [&] {
            auto& closer = install(2, 1);
            run_as(closer);
            semaphore.close();
            run_as(acquiring_thread);
        };
        check(semaphore.acquire(0) == semaphore_result_t::closed, "acquire succeeded after an intervening close");
        check(!on_unmask, "test did not inject the close race");
        semaphore.close_and_wait();
    }

    void test_acquisition_decision_and_increment_are_atomic()
    {
        semaphore_t semaphore { 1, 0 };
        auto        interrupt_result = semaphore_result_t::unsupported;
        // The first unmask leaves the operation guard; the second leaves the
        // acquisition decision. An ISR must not also claim the last permit.
        on_unmask = [&] {
            on_unmask = [&] {
                interrupt_context = true;
                interrupt_result = semaphore.try_acquire();
                interrupt_context = false;
            };
        };
        const auto result = semaphore.acquire(0);
        check(!on_unmask, "test did not interrupt the acquisition decision");
        check(!(result == semaphore_result_t::success && interrupt_result == semaphore_result_t::success),
              "thread and ISR both acquired the single available permit");
        check(result == semaphore_result_t::success || interrupt_result == semaphore_result_t::success,
              "neither caller acquired the available permit");
    }

    void test_release_schedules_under_critical_section()
    {
        semaphore_t semaphore { 1, 1 };
        auto&       waiter = *tcbs[1];
        auto&       releaser = install(2, 12);
        on_block = [&](tcb_t&) {
            run_as(releaser);
            require_masked_schedule = true;
            check(semaphore.release() == semaphore_result_t::success, "release failed");
            require_masked_schedule = false;
        };
        run_as(waiter);
        check(semaphore.acquire(5) == semaphore_result_t::success, "released waiter did not acquire");
    }

    struct fiber_t
    {
        ucontext_t context {};
        alignas(std::max_align_t) std::array<std::byte, 64 * 1024> stack {};
        std::function<void()> action;
        bool                  finished {};
    };

    std::array<fiber_t, build_config::artos::max_threads> fibers {};
    bool                                                  fiber_mode {};
    unsigned                                              fiber_switches {};

    void switch_fiber(tcb_t& incoming)
    {
        check(++fiber_switches <= 64, "lifetime scenario did not make progress");
        const auto outgoing_slot = tcb_t::id_layout::extract_id(current_thread->id);
        const auto incoming_slot = tcb_t::id_layout::extract_id(incoming.id);
        run_as(incoming);
        check(swapcontext(&fibers[outgoing_slot].context, &fibers[incoming_slot].context) == 0,
              "host fiber switch failed");
    }

    void fiber_entry(int slot)
    {
        fibers[slot].action();
        fibers[slot].finished = true;
        --unfinished_workers;
        current_thread->state = thread::state_t::finished;
        {
            const critical_section_t cs {};
            sched();
        }
        check(false, "finished host fiber resumed");
    }

    void start_waiter(semaphore_t& semaphore, std::size_t slot, unsigned priority_value, bool timed)
    {
        auto& waiter = install(slot, priority_value);
        auto& fiber = fibers[slot];
        fiber.action = [&semaphore, timed] {
            const auto result = timed ? semaphore.acquire(100) : semaphore.acquire(wait_forever);
            check(result == semaphore_result_t::closed, "closing did not cancel the waiting acquisition");
            check(current_thread->waiter.queue == nullptr, "cancelled acquisition retained a wait registration");
            check(current_thread->block_sleep.queue == nullptr, "cancelled acquisition retained a deadline");
        };
        check(getcontext(&fiber.context) == 0, "host fiber initialization failed");
        fiber.context.uc_stack.ss_sp = fiber.stack.data();
        fiber.context.uc_stack.ss_size = fiber.stack.size();
        fiber.context.uc_link = nullptr;
        makecontext(&fiber.context, reinterpret_cast<void (*)()>(fiber_entry), 1, static_cast<int>(slot));
        ++unfinished_workers;
        switch_fiber(waiter);
        check(!fiber.finished && waiter.waiter.queue != nullptr, "waiter did not remain blocked");
    }

    void watch(semaphore_t* semaphore)
    {
        watched_allocation = semaphore;
        watched_deallocations = 0;
        deferred_delete_expected = false;
    }

    void check_deleted_once()
    {
        check(watched_deallocations == 1, "close_and_delete returned without deleting the semaphore");
        check(unfinished_workers == 0, "deletion returned before waiting acquisitions finished");
        auto* allocation = std::exchange(watched_allocation, nullptr);
        std::free(allocation); // Release the quarantined allocation without another destructor call.
    }

    void test_close_and_delete_idle()
    {
        auto* semaphore = new semaphore_t { 1, 0 };
        watch(semaphore);
        semaphore->close_and_delete();
        check_deleted_once();
    }

    void test_close_and_delete_already_closed()
    {
        auto* semaphore = new semaphore_t { 1, 0 };
        watch(semaphore);
        semaphore->close();
        semaphore->close_and_delete();
        check_deleted_once();
    }

    void exercise_lifetime(bool delete_object, bool multiple_waiters, bool already_closed = false)
    {
        auto* semaphore = new semaphore_t { 1, 1 };
        watch(semaphore);
        fiber_mode = true;
        start_waiter(*semaphore, 2, 1, true);
        if (multiple_waiters)
        {
            start_waiter(*semaphore, 3, 3, false);
        }
        if (delete_object)
        {
            if (already_closed)
            {
                semaphore->close();
            }
            deferred_delete_expected = true;
            deleting_call_active = true;
            semaphore->close_and_delete();
            deleting_call_active = false;
            check(watched_deallocations == 0, "semaphore deleted while acquisitions were still active");
            check(unfinished_workers == (multiple_waiters ? 2U : 1U),
                  "deferred close unexpectedly waited for acquisitions to finish");
            // close_and_delete only requests destruction. Run the cancelled
            // acquisitions; the last guard must perform the single deletion.
            switch_fiber(*tcbs[2]);
            check_deleted_once();
        }
        else
        {
            semaphore->close_and_wait();
            check(unfinished_workers == 0, "close_and_wait returned before acquisitions finished");
            check(watched_deallocations == 0, "close_and_wait unexpectedly deleted the semaphore");
            check(semaphore->try_acquire() == semaphore_result_t::closed, "drained semaphore was not closed");
            delete semaphore;
            check_deleted_once();
        }
        check(fibers[2].finished, "timed waiter did not finish");
        if (multiple_waiters)
        {
            check(fibers[3].finished, "indefinite waiter did not finish");
        }
        check(current_thread == &*tcbs[1], "lifetime operation returned in the wrong thread");
        check(current_thread->waiter.queue == nullptr, "closer retained a wait registration");
    }

    void test_close_and_wait_drains_one_waiter()
    {
        exercise_lifetime(false, false);
    }

    void test_close_and_wait_drains_multiple_waiters()
    {
        exercise_lifetime(false, true);
    }

    void test_close_and_delete_drains_one_waiter()
    {
        exercise_lifetime(true, false);
    }

    void test_close_and_delete_drains_multiple_waiters()
    {
        exercise_lifetime(true, true);
    }

    void test_close_and_delete_already_closed_with_active_waiter()
    {
        exercise_lifetime(true, false, true);
    }

    void test_delete_request_at_final_operation_unmask()
    {
        auto* semaphore = new semaphore_t { 1, 0 };
        watch(semaphore);
        // Unmask boundaries: operation entry, permit acquisition, then the
        // final guard's decrement to zero. The closer deletes at that third
        // boundary; finish_operation must not read members or delete again.
        on_unmask = [&] { on_unmask = [&] { on_unmask = [&] { semaphore->close_and_delete(); }; }; };
        check(semaphore->acquire(0) == semaphore_result_t::success, "acquisition failed");
        check(!on_unmask, "test did not interrupt the final operation's unmask");
        check_deleted_once();
    }

    void test_constructor_rejects_zero_capacity()
    {
        const semaphore_t semaphore { 0, 0 };
        (void)semaphore;
    }

    void test_constructor_rejects_excess_initial_count()
    {
        const semaphore_t semaphore { 1, 2 };
        (void)semaphore;
    }
}

namespace cpu::port {
    basepri_t irq_save_raise_mask(basepri_t priority_value) noexcept
    {
        check(!deleting_call_active || watched_deallocations == 0,
              "close_and_delete continued kernel access after deleting itself");
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
        if (interrupt_mask == 0 && on_unmask && !interrupt_context)
        {
            auto handler = std::exchange(on_unmask, {});
            handler();
        }
        if (interrupt_mask != 0 || !current_thread
            || (current_thread->state != thread::state_t::blocked
                && current_thread->state != thread::state_t::indefinite
                && !(fiber_mode && current_thread->state == thread::state_t::finished)))
        {
            return;
        }

        if (fiber_mode)
        {
            check(next_thread != nullptr && next_thread != current_thread,
                  "fiber scheduler did not select another thread");
            switch_fiber(*static_cast<tcb_t*>(next_thread));
            return;
        }

        // Simulate the context switch only after the blocking critical section
        // releases BASEPRI. The production blocking, wake and scheduler code is
        // linked unchanged; a scenario runs other threads until the waiter wakes.
        ++block_count;
        check(block_count <= 4, "semaphore wait did not make progress");
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
        check(!require_masked_schedule || interrupt_mask != 0,
              "release called the scheduler without holding a critical section");
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
        int expected_signal = 0;
    };

    const std::array tests {
        test_t { "constructed_open", test_constructed_open },
        test_t { "initial_permits_in_use", test_initial_permits_in_use },
        test_t { "release_frees_permit", test_release_frees_permit },
        test_t { "release_when_unused", test_release_when_unused },
        test_t { "zero_wait", test_zero_wait },
        test_t { "timed_wake", test_timed_wake },
        test_t { "indefinite_isr_wake", test_indefinite_isr_wake },
        test_t { "wake_flag_accumulates", test_wake_flag_accumulates },
        test_t { "null_wake_pointer", test_null_wake_pointer },
        test_t { "isr_try_acquire", test_isr_try_acquire },
        test_t { "isr_blocking_acquire_rejected", test_isr_blocking_acquire_rejected },
        test_t { "isr_full_acquire_rejected", test_isr_full_acquire_rejected },
        test_t { "maximum_count_does_not_wrap", test_maximum_count_does_not_wrap },
        test_t { "waiter_does_not_donate_priority", test_waiter_does_not_donate_priority },
        test_t { "timeout", test_timeout },
        test_t { "deadline_rollover", test_deadline_rollover },
        test_t { "retry_keeps_deadline", test_retry_keeps_deadline },
        test_t { "close_rejects_operations", test_close_rejects_operations },
        test_t { "close_cancels_waiters", test_close_cancels_waiters },
        test_t { "close_between_entry_and_acquisition", test_close_between_entry_and_acquisition },
        test_t { "acquisition_decision_and_increment_are_atomic", test_acquisition_decision_and_increment_are_atomic },
        test_t { "release_schedules_under_critical_section", test_release_schedules_under_critical_section },
        test_t { "close_and_delete_idle", test_close_and_delete_idle },
        test_t { "close_and_delete_already_closed", test_close_and_delete_already_closed },
        test_t { "close_and_wait_drains_one_waiter", test_close_and_wait_drains_one_waiter },
        test_t { "close_and_wait_drains_multiple_waiters", test_close_and_wait_drains_multiple_waiters },
        test_t { "close_and_delete_drains_one_waiter", test_close_and_delete_drains_one_waiter },
        test_t { "close_and_delete_drains_multiple_waiters", test_close_and_delete_drains_multiple_waiters },
        test_t { "close_and_delete_already_closed_with_active_waiter",
                 test_close_and_delete_already_closed_with_active_waiter },
        test_t { "delete_request_at_final_operation_unmask", test_delete_request_at_final_operation_unmask },
        test_t { "constructor_rejects_zero_capacity", test_constructor_rejects_zero_capacity, SIGABRT },
        test_t { "constructor_rejects_excess_initial_count", test_constructor_rejects_excess_initial_count, SIGABRT },
    };
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
        const bool passed = test.expected_signal != 0 ? WIFSIGNALED(status) && WTERMSIG(status) == test.expected_signal
                                                      : WIFEXITED(status) && WEXITSTATUS(status) == 0;
        std::printf("%s: %s\n", passed ? "PASS" : "FAIL", test.name);
        failures += !passed;
    }
    std::printf("%zu tests, %u failed\n", tests.size(), failures);
    return failures == 0 ? 0 : 1;
}
