#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

#include <ucontext.h>

#include <artos.h>
#include <debug_test_suite.h>
#include <event.h>
#include <mutex.h>
#include <queue.h>
#include <semaphore.h>

#include <internal/kernel_state.h>
#include <internal/operation.h>
#include <internal/scheduler.h>
#include <internal/thread.h>

namespace {
    void*    watched_allocation {};
    unsigned deallocations {};

    void deallocate(void* allocation) noexcept
    {
        if (allocation && allocation == watched_allocation)
        {
            ++deallocations;
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
    deallocate(allocation);
}

void operator delete(void* allocation, std::size_t) noexcept
{
    deallocate(allocation);
}

namespace {
    using namespace artos;
    using namespace artos::kernel::internal;
    using namespace artos::thread::internal;
    using host_tests::check;

    std::function<void()> on_unmask;

    struct fiber_t
    {
        ucontext_t context {};
        alignas(std::max_align_t) std::array<std::byte, 64 * 1024> stack {};
        std::function<void()> action;
    };

    std::array<std::optional<tcb_t>, build_config::artos::max_threads> tcbs {};
    std::array<thread::stack_word_t, build_config::artos::max_threads> stacks {};
    std::array<fiber_t, build_config::artos::max_threads>              fibers {};
    tick_time_t                                                        now {};
    cpu::port::basepri_t                                               mask {};
    bool                                                               in_irq {};
    unsigned                                                           switches {};
    unsigned                                                           schedule_requests {};

    tcb_t& install(std::size_t slot, unsigned priority)
    {
        auto& tcb = tcbs[slot].emplace(
            "kill test", static_cast<thread::priority_t>(priority), &stacks[slot], &stacks[slot], 1);
        (void)tcb.make_thread_id(slot);
        threads[slot] = &tcb;
        ++thread_count;
        make_thread_ready(&tcb);
        return tcb;
    }

    void run_as(tcb_t& incoming)
    {
        if (current_thread && current_thread->state == thread::state_t::running)
        {
            current_thread->state = thread::state_t::ready;
        }
        assert(incoming.ready.queue);
        current_thread = &incoming;
        incoming.state = thread::state_t::running;
    }

    void switch_to(tcb_t& incoming)
    {
        ++switches;
        assert(switches < 128);
        const auto outgoing_slot = tcb_t::id_layout::extract_id(current_thread->id);
        const auto incoming_slot = tcb_t::id_layout::extract_id(incoming.id);
        run_as(incoming);
        const auto result = swapcontext(&fibers[outgoing_slot].context, &fibers[incoming_slot].context);
        assert(result == 0);
    }

    void entry(int slot)
    {
        fibers[slot].action();
        (void)thread::kill(current_thread->id);
        std::abort(); // A killed task must not resume.
    }

    void start(std::size_t slot, std::function<void()> action)
    {
        auto& tcb = install(slot, 2);
        auto& fiber = fibers[slot];
        fiber.action = std::move(action);
        const auto result = getcontext(&fiber.context);
        assert(result == 0);
        fiber.context.uc_stack.ss_sp = fiber.stack.data();
        fiber.context.uc_stack.ss_size = fiber.stack.size();
        fiber.context.uc_link = nullptr;
        makecontext(&fiber.context, reinterpret_cast<void (*)()>(entry), 1, static_cast<int>(slot));
        switch_to(tcb);
    }

    void reset()
    {
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
        for (auto& tcb : tcbs)
        {
            tcb.reset();
        }
        for (auto& fiber : fibers)
        {
            fiber.action = {};
        }
        now = {};
        mask = 0;
        in_irq = false;
        switches = 0;
        schedule_requests = 0;
        watched_allocation = nullptr;
        deallocations = 0;
        on_unmask = {};
    }

    struct fixture_t
    {
        fixture_t()
        {
            reset();
            (void)install(0, 15);
            run_as(install(1, 9));
        }

        ~fixture_t()
        {
            reset();
        }
    };

    enum class kind_t
    {
        event,
        semaphore,
        reader,
        writer
    };

    template<kind_t Kind>
    struct resource_t
    {
        using object_t = std::conditional_t<Kind == kind_t::event,
                                            event_t,
                                            std::conditional_t<Kind == kind_t::semaphore, semaphore_t, queue_t<int>>>;
        queue_storage_t<int, 1> storage {};
        object_t*               object;

        resource_t()
        {
            if constexpr (Kind == kind_t::event)
            {
                object = new event_t;
            }
            else if constexpr (Kind == kind_t::semaphore)
            {
                object = new semaphore_t(1, 1);
            }
            else
            {
                object = new queue_t<int>(storage);
                if constexpr (Kind == kind_t::writer)
                {
                    (void)object->try_push_back(7);
                }
            }
            watched_allocation = object;
        }

        ~resource_t()
        {
            delete object;
        }

        bool wait(bool timed)
        {
            if constexpr (Kind == kind_t::event)
            {
                return (timed ? object->wait_for(any_t { 1 }, false, tick_t { 10 })
                              : object->wait_for(any_t { 1 }, false, wait_forever))
                    .has_value();
            }
            else if constexpr (Kind == kind_t::semaphore)
            {
                return (timed ? object->acquire(tick_t { 10 }) : object->acquire(wait_forever))
                       == semaphore_result_t::success;
            }
            else
            {
                if constexpr (Kind == kind_t::reader)
                {
                    int element = 8;
                    return (timed ? object->pop_front(element, tick_t { 10 })
                                  : object->pop_front(element, wait_forever))
                           == queue_result_t::success;
                }
                else
                {
                    const int element = 8;
                    return (timed ? object->push_back(element, tick_t { 10 })
                                  : object->push_back(element, wait_forever))
                           == queue_result_t::success;
                }
            }
        }

        void signal()
        {
            if constexpr (Kind == kind_t::event)
            {
                (void)object->post(1);
            }
            else if constexpr (Kind == kind_t::semaphore)
            {
                (void)object->release();
            }
            else if constexpr (Kind == kind_t::reader)
            {
                (void)object->try_push_back(7);
            }
            else
            {
                int element {};
                (void)object->try_pop_front(element);
            }
        }

        void request_delete()
        {
            object->close_and_delete();
            object = nullptr;
        }
    };

    template<kind_t Kind, bool Timed, bool Signal = false, bool DeleteFirst = false, bool Expire = false>
    void test_active_operation()
    {
        const fixture_t  fixture;
        resource_t<Kind> resource;
        bool             returned = false;
        start(2, [&] {
            (void)resource.wait(Timed);
            returned = true;
        });
        auto& target = *tcbs[2];
        check(target.active_operations && target.waiter.queue, "blocked call has no cleanup registration");
        if constexpr (Signal)
        {
            resource.signal();
        }
        if constexpr (Expire)
        {
            now = target.deadline;
            const critical_section_t cs;
            sched();
        }
        if constexpr (DeleteFirst)
        {
            resource.request_delete();
            check(deallocations == 0, "deferred deletion did not retain active call");
        }
        target.suspended = Timed;
        const auto  state = target.state;
        auto* const registration = target.active_operations;
        auto* const wait_queue = target.waiter.queue;
        auto* const ready = target.ready.queue;
        auto* const deadline_queue = target.block_sleep.queue;
        const auto  requests = schedule_requests;
        check(thread::try_kill(target.id) == thread::kill_result_t::busy, "try_kill accepted an active call");
        check(target.state == state && target.suspended == Timed && target.active_operations == registration
                  && target.waiter.queue == wait_queue && target.ready.queue == ready
                  && target.block_sleep.queue == deadline_queue && schedule_requests == requests,
              "busy try_kill changed its target or requested scheduling");
        check(thread::kill(target.id) == thread::kill_result_t::success, "forced kill failed");
        check(target.state == thread::state_t::finished && !target.active_operations && !target.waiter.queue
                  && !target.ready.queue && !target.block_sleep.queue && !target.waiting_event && !target.waiting_mutex
                  && !returned,
              "forced kill left a live registration, kernel link, or resumed the abandoned call");
        if constexpr (!DeleteFirst)
        {
            resource.object->close_and_wait();
            resource.request_delete();
        }
        check(deallocations == 1, "killed operation did not allow exactly-once deletion");
        check(thread::kill(target.id) == thread::kill_result_t::success
                  && thread::try_kill(target.id) == thread::kill_result_t::success && deallocations == 1,
              "repeat termination changed cleanup results");
    }

    template<kind_t Kind>
    void test_normal_completion()
    {
        const fixture_t  fixture;
        resource_t<Kind> resource;
        bool             succeeded = false;
        start(2, [&] {
            succeeded = resource.wait(true);
            switch_to(*tcbs[1]); // Pause application code after the guard exits.
            std::abort();
        });
        resource.signal();
        switch_to(*tcbs[2]);
        check(succeeded && !tcbs[2]->active_operations, "normal return did not unregister its operation");
        check(thread::try_kill(tcbs[2]->id) == thread::kill_result_t::success,
              "try_kill rejected a task after normal guard completion");
        resource.object->close_and_wait();
        resource.request_delete();
        check(deallocations == 1, "normal completion left duplicate cleanup or leaked the resource");
    }

    template<kind_t Kind>
    void test_last_waiter_deletes()
    {
        const fixture_t  fixture;
        resource_t<Kind> resource;
        bool             first_returned = false;
        bool             second_returned = false;
        start(2, [&] {
            (void)resource.wait(true);
            first_returned = true;
        });
        start(3, [&] {
            (void)resource.wait(false);
            second_returned = true;
        });
        resource.request_delete();
        check(thread::kill(tcbs[2]->id) == thread::kill_result_t::success && deallocations == 0,
              "killing one waiter deleted an object still used by another");
        switch_to(*tcbs[3]);
        check(!first_returned && second_returned && deallocations == 1 && !tcbs[3]->active_operations,
              "the surviving wait did not perform final deferred deletion");
    }

    void test_pending_mutex_donation()
    {
        const fixture_t fixture;
        mutex_t         mutex;
        check(mutex.try_lock() == mutex_result_t::success, "could not set up mutex owner");
        start(2, [&] { (void)mutex.lock(wait_forever); });
        check(tcbs[1]->priority == tcbs[2]->priority, "pending mutex did not donate priority");
        check(thread::try_kill(tcbs[2]->id) == thread::kill_result_t::success && !tcbs[2]->waiting_mutex
                  && !tcbs[2]->waiter.queue && tcbs[1]->priority == tcbs[1]->base_priority,
              "terminating a mutex waiter did not remove its donation");
        check(mutex.release() == mutex_result_t::success, "owner could not release after waiter termination");
    }

    void test_isr_guard_keeps_interrupted_registration()
    {
        const fixture_t     fixture;
        semaphore_t         semaphore(1, 1);
        operation_cleanup_t parent;
        unsigned            completed = 0;
        {
            const critical_section_t cs;
            register_operation(parent, &completed, [](void* context) noexcept { ++*static_cast<unsigned*>(context); });
        }
        bool unchanged = false;
        on_unmask = [&] { unchanged = current_thread->active_operations == &parent; };
        in_irq = true;
        const auto result = semaphore.release_from_isr();
        in_irq = false;
        check(unchanged && result == semaphore_result_t::success && current_thread->active_operations == &parent,
              "ISR guard was registered against the interrupted task");
        complete_operation(parent);
        check(completed == 1 && !current_thread->active_operations, "ISR completion changed the task registration");
        semaphore.close_and_wait();
    }

    void test_try_self_without_operations()
    {
        const fixture_t fixture;
        bool            returned = false;
        start(2, [&] {
            (void)thread::try_kill(current_thread->id);
            returned = true;
            std::abort();
        });
        check(!returned && tcbs[2]->state == thread::state_t::finished,
              "successful self try_kill resumed application code");
    }

    void test_admission_and_ids()
    {
        const fixture_t fixture;
        auto&           target = install(2, 2);
        in_irq = true;
        const auto irq_try = thread::try_kill(target.id);
        const auto irq_force = thread::kill(target.id);
        in_irq = false;
        check(irq_try == thread::kill_result_t::unsupported && irq_force == thread::kill_result_t::unsupported
                  && target.state == thread::state_t::ready,
              "termination accepted IRQ context");
        for (const auto id : { thread::invalid_id, tcbs[0]->id, tcb_t::id_layout::next(target.id, 2) })
        {
            check(thread::try_kill(id) == thread::kill_result_t::invalid_thread
                      && thread::kill(id) == thread::kill_result_t::invalid_thread,
                  "termination accepted an idle, invalid, or stale ID");
        }
        target.suspended = true;
        check(thread::try_kill(target.id) == thread::kill_result_t::success && target.state == thread::state_t::finished
                  && !target.ready.queue,
              "try_kill did not terminate an unguarded suspended task");
    }

    void test_mutex_owner_busy()
    {
        const fixture_t fixture;
        mutex_t         mutex;
        auto&           owner = install(2, 2);
        run_as(owner);
        const auto acquired = mutex.try_lock();
        run_as(*tcbs[1]);
        check(acquired == mutex_result_t::success, "mutex fixture failed");
        check(thread::try_kill(owner.id) == thread::kill_result_t::busy && owner.state == thread::state_t::ready
                  && !owner.mutexes.empty(),
              "try_kill changed a mutex owner");
        run_as(owner);
        const auto released = mutex.release();
        run_as(*tcbs[1]);
        check(released == mutex_result_t::success && thread::try_kill(owner.id) == thread::kill_result_t::success,
              "try_kill remained busy after mutex release");
    }

    void test_forced_mutex_owner()
    {
        const fixture_t fixture;
        mutex_t         mutex;
        auto&           owner = install(2, 2);
        run_as(owner);
        const auto acquired = mutex.try_lock();
        run_as(*tcbs[1]);
        check(acquired == mutex_result_t::success && thread::kill(owner.id) == thread::kill_result_t::success,
              "forced kill did not accept a mutex owner");
        check(!owner.mutexes.empty() && mutex.try_lock() == mutex_result_t::timeout,
              "forced kill silently released a mutex protecting potentially inconsistent data");
        // Neither this mutex nor its owner's storage is reused after the kill.
    }

    void test_nested_cleanup_before_join()
    {
        const fixture_t fixture;
        auto&           target = install(2, 2);
        auto&           joiner = install(3, 3);
        run_as(joiner);
        {
            const critical_section_t cs;
            kernel::locked::block_current(target.joiners, wait_forever);
            run_as(target);
        }
        operation_cleanup_t first {}, second {}, third {};

        struct observation_t
        {
            unsigned count {};
            bool     detached { true };
        } observation;

        const auto cleanup = [](void* context) noexcept {
            auto& observed = *static_cast<observation_t*>(context);
            ++observed.count;
            observed.detached &= tcbs[2]->state == thread::state_t::finished && !tcbs[2]->ready.queue
                                 && tcbs[3]->waiter.queue == &tcbs[2]->joiners;
        };
        {
            const critical_section_t cs;
            register_operation(first, &observation, cleanup);
            register_operation(second, &observation, cleanup);
        }
        // Exercise normal removal of a node that is not at the list head.
        complete_operation(first);
        observation.count = 0;
        observation.detached = true;
        {
            const critical_section_t cs;
            register_operation(third, &observation, cleanup);
        }
        run_as(*tcbs[1]);
        check(thread::try_kill(target.id) == thread::kill_result_t::busy, "nested cleanup was not tracked");
        check(thread::kill(target.id) == thread::kill_result_t::success && observation.count == 2
                  && observation.detached && !target.active_operations && target.joiners.empty()
                  && joiner.state == thread::state_t::ready,
              "cleanup did not precede join completion");
        complete_operation(second);
        complete_operation(third);
        check(observation.count == 2, "abandoned guard could finish twice");
    }

    void test_self_kill()
    {
        const fixture_t fixture;
        unsigned        cleaned = 0;
        bool            busy = false;
        bool            returned = false;
        start(2, [&] {
            operation_cleanup_t operation;
            {
                const critical_section_t cs;
                register_operation(
                    operation, &cleaned, [](void* context) noexcept { ++*static_cast<unsigned*>(context); });
            }
            busy = thread::try_kill(current_thread->id) == thread::kill_result_t::busy;
            (void)thread::kill(current_thread->id);
            returned = true;
            std::abort();
        });
        check(busy && cleaned == 1 && !returned && tcbs[2]->state == thread::state_t::finished
                  && !tcbs[2]->active_operations && !tcbs[2]->ready.queue,
              "self kill did not reject safely or clean up before switching away");
    }
}

namespace cpu::port {
    basepri_t irq_save_raise_mask(basepri_t priority) noexcept
    {
        const auto previous = mask;
        if (mask == 0 || priority < mask)
        {
            mask = priority;
        }
        return previous;
    }

    void irq_restore_mask(basepri_t previous) noexcept
    {
        mask = previous;
        if (mask == 0 && on_unmask)
        {
            auto callback = std::exchange(on_unmask, {});
            callback();
        }
        if (mask == 0 && !in_irq && current_thread
            && (current_thread->state == artos::thread::state_t::blocked
                || current_thread->state == artos::thread::state_t::indefinite
                || current_thread->state == artos::thread::state_t::finished))
        {
            assert(next_thread && next_thread != current_thread);
            switch_to(*static_cast<tcb_t*>(next_thread));
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
        ++schedule_requests;
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
        host_tests::test_case_t { "forced kill retains mutex ownership", test_forced_mutex_owner },
        host_tests::test_case_t { "event last waiter deletes", test_last_waiter_deletes<kind_t::event> },
        host_tests::test_case_t { "semaphore last waiter deletes", test_last_waiter_deletes<kind_t::semaphore> },
        host_tests::test_case_t { "queue reader last waiter deletes", test_last_waiter_deletes<kind_t::reader> },
        host_tests::test_case_t { "queue writer last waiter deletes", test_last_waiter_deletes<kind_t::writer> },
        host_tests::test_case_t { "try_kill removes pending mutex donation", test_pending_mutex_donation },
        host_tests::test_case_t { "ISR guard does not register interrupted task",
                                  test_isr_guard_keeps_interrupted_registration },
        host_tests::test_case_t { "try_kill self without operations", test_try_self_without_operations },
        host_tests::test_case_t { "termination admission and IDs", test_admission_and_ids },
        host_tests::test_case_t { "try_kill rejects mutex owner", test_mutex_owner_busy },
        host_tests::test_case_t { "cleanup before join and nested registration", test_nested_cleanup_before_join },
        host_tests::test_case_t { "self termination", test_self_kill },
        host_tests::test_case_t { "kill timed event", test_active_operation<kind_t::event, true> },
        host_tests::test_case_t { "kill indefinite semaphore", test_active_operation<kind_t::semaphore, false> },
        host_tests::test_case_t { "kill timed queue reader", test_active_operation<kind_t::reader, true> },
        host_tests::test_case_t { "kill indefinite queue writer", test_active_operation<kind_t::writer, false> },
        host_tests::test_case_t { "kill signaled event", test_active_operation<kind_t::event, true, true> },
        host_tests::test_case_t { "kill signaled semaphore", test_active_operation<kind_t::semaphore, true, true> },
        host_tests::test_case_t { "kill signaled queue reader", test_active_operation<kind_t::reader, false, true> },
        host_tests::test_case_t { "kill signaled queue writer", test_active_operation<kind_t::writer, true, true> },
        host_tests::test_case_t { "kill deleting event", test_active_operation<kind_t::event, false, false, true> },
        host_tests::test_case_t { "kill deleting semaphore",
                                  test_active_operation<kind_t::semaphore, true, false, true> },
        host_tests::test_case_t { "kill deleting queue reader",
                                  test_active_operation<kind_t::reader, true, false, true> },
        host_tests::test_case_t { "kill deleting queue writer",
                                  test_active_operation<kind_t::writer, false, false, true> },
        host_tests::test_case_t { "kill timed-out event",
                                  test_active_operation<kind_t::event, true, false, false, true> },
        host_tests::test_case_t { "kill timed-out semaphore",
                                  test_active_operation<kind_t::semaphore, true, false, false, true> },
        host_tests::test_case_t { "kill timed-out queue reader",
                                  test_active_operation<kind_t::reader, true, false, false, true> },
        host_tests::test_case_t { "kill timed-out queue writer",
                                  test_active_operation<kind_t::writer, true, false, false, true> },
        host_tests::test_case_t { "event normal completion", test_normal_completion<kind_t::event> },
        host_tests::test_case_t { "semaphore normal completion", test_normal_completion<kind_t::semaphore> },
        host_tests::test_case_t { "queue reader normal completion", test_normal_completion<kind_t::reader> },
        host_tests::test_case_t { "queue writer normal completion", test_normal_completion<kind_t::writer> },
    };
    return host_tests::run_cases(argc, argv, cases);
}
