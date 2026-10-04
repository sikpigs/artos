#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <optional>
#include <utility>

#include <sys/resource.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#include <artos.h>
#include <queue.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>
#include <internal/thread.h>

namespace {
    void*    watched_allocation {};
    unsigned watched_deallocations {};
    unsigned unfinished_workers {};
    unsigned watched_destructors {};
    bool     deleting_call_active {};
    bool     deferred_delete_expected {};

    void observe_delete(void* allocation) noexcept
    {
        if (allocation && allocation == watched_allocation)
        {
            ++watched_deallocations;
            // Deferred deletion runs in the final operation's guard
            // destructor, before that worker returns from push/pop.
            const auto expected_workers = deferred_delete_expected ? 1U : 0U;
            if (unfinished_workers != expected_workers || watched_deallocations != 1 || watched_destructors != 1)
            {
                std::fprintf(stderr, "FAIL: queue deleted at the wrong operation boundary, or deleted twice\n");
                std::exit(1);
            }
            // Quarantine this allocation until the scenario finishes. The port
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
        auto& thread = tcbs[slot].emplace("queue lifetime", priority(priority_value), &stacks[slot], &stacks[slot], 1);
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

    // The base destructor is virtual: deletion through queue_core_t must also
    // destroy this derived object, exactly once.
    class observed_queue_t final : public queue_t<int>
    {
    public:
        using queue_t<int>::queue_t;

        ~observed_queue_t() noexcept override
        {
            if (this == watched_allocation)
            {
                ++watched_destructors;
            }
        }
    };

    template<typename Queue>
    constexpr bool has_delete_api = requires(Queue& queue) { queue.close_and_delete(); };

    template<typename Queue>
    void request_delete(Queue* queue)
    {
        // Keep the upcoming tests buildable before the public wrapper exists.
        // The runner reports these cases as pending, never as passed.
        if constexpr (has_delete_api<Queue>)
        {
            queue->close_and_delete();
        }
        else
        {
            check(false, "queue_t::close_and_delete is not public yet");
        }
    }

    void watch(observed_queue_t* queue)
    {
        watched_allocation = queue;
        watched_deallocations = 0;
        watched_destructors = 0;
        deferred_delete_expected = false;
    }

    void check_deleted_once()
    {
        check(watched_deallocations == 1 && watched_destructors == 1,
              "queue was not destroyed and deallocated exactly once");
        check(unfinished_workers == 0, "scenario ended before queue users finished");
        std::free(std::exchange(watched_allocation, nullptr));
    }

    enum class operation_t
    {
        push_front,
        push_back,
        pop_front,
        pop_back
    };

    bool is_writer(operation_t operation)
    {
        return operation == operation_t::push_front || operation == operation_t::push_back;
    }

    template<typename Wait>
    queue_result_t operate(observed_queue_t& queue, operation_t operation, int& element, Wait wait)
    {
        switch (operation)
        {
            case operation_t::push_front:
                return queue.push_front(element, wait);
            case operation_t::push_back:
                return queue.push_back(element, wait);
            case operation_t::pop_front:
                return queue.pop_front(element, wait);
            case operation_t::pop_back:
                return queue.pop_back(element, wait);
        }
        std::abort();
    }

    void start_waiter(
        observed_queue_t& queue, std::size_t slot, operation_t operation, bool timed, unsigned priority_value = 1)
    {
        auto& waiter = install(slot, priority_value);
        auto& fiber = fibers[slot];
        fiber.action = [&queue, operation, timed] {
            int        element = 42;
            const auto result = timed ? operate(queue, operation, element, tick_t { 100 })
                                      : operate(queue, operation, element, wait_forever);
            check(result == queue_result_t::closed, "cancelled push/pop did not return closed");
            check(element == 42, "cancelled push/pop changed its element");
            check(current_thread->waiter.queue == nullptr, "cancelled operation retained a queue registration");
            check(current_thread->block_sleep.queue == nullptr, "cancelled operation retained a deadline");
        };
        check(getcontext(&fiber.context) == 0, "host fiber initialization failed");
        fiber.context.uc_stack.ss_sp = fiber.stack.data();
        fiber.context.uc_stack.ss_size = fiber.stack.size();
        fiber.context.uc_link = nullptr;
        makecontext(&fiber.context, reinterpret_cast<void (*)()>(fiber_entry), 1, static_cast<int>(slot));
        ++unfinished_workers;
        switch_fiber(waiter);
        check(!fiber.finished && waiter.waiter.queue != nullptr, "operation did not remain blocked");
    }

    void finish_waiters()
    {
        for (std::size_t slot = 2; slot <= 3; ++slot)
        {
            if (tcbs[slot] && !fibers[slot].finished)
            {
                switch_fiber(*tcbs[slot]);
            }
        }
        check(unfinished_workers == 0, "cancelled queue operations did not finish");
        check(current_thread == &*tcbs[1], "scenario resumed the wrong thread");
    }

    template<bool Full, bool Closed>
    void test_idle_delete()
    {
        queue_storage_t<int, 1> storage {};
        auto*                   queue = new observed_queue_t { storage };
        watch(queue);
        if constexpr (Full)
        {
            check(queue->try_push_back(7) == queue_result_t::success, "queue setup failed");
        }
        const auto saved_buffer = storage.buffer;
        if constexpr (Closed)
        {
            queue->close();
        }
        deleting_call_active = true;
        request_delete(queue);
        deleting_call_active = false;
        check_deleted_once();
        check(storage.buffer == saved_buffer, "deleting the queue modified caller-owned storage");
        // The queue must not deallocate or retain ownership of external storage.
        queue_t<int> replacement { storage };
        check(replacement.try_push_front(9) == queue_result_t::success, "storage could not be reused");
        int output = 0;
        check(replacement.try_pop_back(output) == queue_result_t::success && output == 9,
              "replacement queue could not read caller-owned storage");
    }

    template<operation_t Operation, bool Timed, bool Multiple = false, bool AlreadyClosed = false, bool Delete = true>
    void test_waiters()
    {
        queue_storage_t<int, 1> storage {};
        auto*                   queue = new observed_queue_t { storage };
        watch(queue);
        if (is_writer(Operation))
        {
            check(queue->try_push_back(7) == queue_result_t::success, "full queue setup failed");
        }
        const auto saved_buffer = storage.buffer;
        fiber_mode = true;
        start_waiter(*queue, 2, Operation, Timed);
        if constexpr (Multiple)
        {
            // Opposite end and opposite wait mode, on the same wait queue.
            const auto other = Operation == operation_t::push_front  ? operation_t::push_back
                               : Operation == operation_t::push_back ? operation_t::push_front
                               : Operation == operation_t::pop_front ? operation_t::pop_back
                                                                     : operation_t::pop_front;
            start_waiter(*queue, 3, other, !Timed, 3);
        }
        if constexpr (AlreadyClosed)
        {
            queue->close();
        }
        if constexpr (Delete)
        {
            deferred_delete_expected = true;
            deleting_call_active = true;
            request_delete(queue);
            deleting_call_active = false;
            check(watched_deallocations == 0 && watched_destructors == 0,
                  "queue deleted while operations were still active");
            check(unfinished_workers == (Multiple ? 2U : 1U), "delete request unexpectedly waited");
            finish_waiters();
        }
        else
        {
            queue->close_and_wait();
            check(watched_deallocations == 0 && watched_destructors == 0, "close_and_wait deleted the queue");
            finish_waiters();
            int output = 42;
            check(queue->try_pop_front(output) == queue_result_t::closed && output == 42,
                  "drained queue did not remain closed");
            delete queue;
        }
        check_deleted_once();
        check(storage.buffer == saved_buffer, "cancelled operations modified queue storage");
    }

    void test_mixed_pending_reader_and_writer()
    {
        queue_storage_t<int, 1> storage {};
        auto*                   queue = new observed_queue_t { storage };
        watch(queue);
        fiber_mode = true;
        // Wake a lower-priority reader without running it, then block a writer
        // on the newly full queue. Both operations remain active when closed.
        start_waiter(*queue, 2, operation_t::pop_front, true, 12);
        check(queue->try_push_back(7) == queue_result_t::success, "reader wake setup failed");
        start_waiter(*queue, 3, operation_t::push_back, false);
        const auto saved_buffer = storage.buffer;
        deferred_delete_expected = true;
        request_delete(queue);
        check(watched_deallocations == 0 && unfinished_workers == 2, "mixed users were not deferred");
        finish_waiters();
        check_deleted_once();
        check(storage.buffer == saved_buffer, "closing mixed users changed buffered data");
    }

    template<operation_t Operation>
    void test_delete_at_final_operation_unmask()
    {
        queue_storage_t<int, 1> storage {};
        auto*                   queue = new observed_queue_t { storage };
        watch(queue);
        if (!is_writer(Operation))
        {
            check(queue->try_push_back(7) == queue_result_t::success, "pop setup failed");
        }
        // Entry, push/pop decision, then the final guard's decrement to zero.
        on_unmask = [&] { on_unmask = [&] { on_unmask = [&] { request_delete(queue); }; }; };
        int element = 42;
        check(operate(*queue, Operation, element, tick_t { 0 }) == queue_result_t::success,
              "operation interrupted by deletion did not succeed");
        check(!on_unmask, "final-operation deletion was not injected");
        check_deleted_once();
    }

    void test_normal_operations_do_not_delete()
    {
        queue_storage_t<int, 1> storage {};
        auto*                   queue = new observed_queue_t { storage };
        watch(queue);
        int element = 7;
        for (auto operation :
             { operation_t::push_front, operation_t::pop_back, operation_t::push_back, operation_t::pop_front })
        {
            check(operate(*queue, operation, element, tick_t { 0 }) == queue_result_t::success,
                  "ordinary queue operation failed");
            check(watched_deallocations == 0 && watched_destructors == 0, "ordinary operation deleted the queue");
        }
        queue->close();
        check(watched_destructors == 0, "plain close destroyed the queue");
        delete queue;
        check_deleted_once();
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

        check(false, "unexpected blocking call without host fibers");
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
    using enum operation_t;

    struct test_t
    {
        const char* name;
        void (*run)();
        bool needs_delete = true;
    };

    const std::array tests {
        test_t { "normal_operations_do_not_delete", test_normal_operations_do_not_delete, false },
        test_t { "close_and_wait_reader", test_waiters<pop_front, true, false, false, false>, false },
        test_t { "close_and_wait_readers", test_waiters<pop_back, false, true, false, false>, false },
        test_t { "close_and_wait_writer", test_waiters<push_front, true, false, false, false>, false },
        test_t { "close_and_wait_writers", test_waiters<push_back, false, true, false, false>, false },
        test_t { "idle_empty", test_idle_delete<false, false> },
        test_t { "idle_full", test_idle_delete<true, false> },
        test_t { "already_closed_empty", test_idle_delete<false, true> },
        test_t { "already_closed_full", test_idle_delete<true, true> },
        test_t { "timed_push_front", test_waiters<push_front, true> },
        test_t { "timed_push_back", test_waiters<push_back, true> },
        test_t { "timed_pop_front", test_waiters<pop_front, true> },
        test_t { "timed_pop_back", test_waiters<pop_back, true> },
        test_t { "forever_push_front", test_waiters<push_front, false> },
        test_t { "forever_push_back", test_waiters<push_back, false> },
        test_t { "forever_pop_front", test_waiters<pop_front, false> },
        test_t { "forever_pop_back", test_waiters<pop_back, false> },
        test_t { "multiple_readers", test_waiters<pop_front, true, true> },
        test_t { "multiple_writers", test_waiters<push_front, true, true> },
        test_t { "already_closed_active_readers", test_waiters<pop_back, false, true, true> },
        test_t { "already_closed_active_writers", test_waiters<push_back, false, true, true> },
        test_t { "mixed_pending_reader_and_writer", test_mixed_pending_reader_and_writer },
        test_t { "final_unmask_push_front", test_delete_at_final_operation_unmask<push_front> },
        test_t { "final_unmask_push_back", test_delete_at_final_operation_unmask<push_back> },
        test_t { "final_unmask_pop_front", test_delete_at_final_operation_unmask<pop_front> },
        test_t { "final_unmask_pop_back", test_delete_at_final_operation_unmask<pop_back> },
    };
    const rlimit core_limit { 0, 0 };
    check(setrlimit(RLIMIT_CORE, &core_limit) == 0, "could not disable core dumps");
    unsigned failures = 0;
    unsigned pending = 0;
    for (const auto& test : tests)
    {
        if (test.needs_delete && !has_delete_api<observed_queue_t>)
        {
            ++pending;
            std::printf("PENDING: %s (public close_and_delete API not available)\n", test.name);
            continue;
        }
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
    std::printf("%zu tests, %u failed, %u pending\n", tests.size(), failures, pending);
    return failures ? 1 : pending ? 77 : 0;
}
