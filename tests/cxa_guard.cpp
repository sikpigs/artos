#include <artos.h>
#include <port/cpu_port.h>
#include <wait.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

extern "C" int  __cxa_guard_acquire(volatile std::uint32_t* guard);
extern "C" void __cxa_guard_release(volatile std::uint32_t* guard);
extern "C" void __cxa_guard_abort(volatile std::uint32_t* guard);

namespace {
    constexpr std::uint32_t initialized = 1U;
    constexpr std::uint32_t in_progress = 2U;
    constexpr int           fatal_exit = 86;

    artos::thread::id_t  current_thread_id = 1;
    bool                 in_irq = false;
    cpu::port::primask_t interrupt_state = 0;
    unsigned             barrier_count = 0;
    unsigned             block_count = 0;
    unsigned             wake_all_count = 0;
    unsigned             schedule_count = 0;
    bool                 wake_higher_priority = false;
    void (*on_block)() = nullptr;

    volatile std::uint32_t* blocked_guard = nullptr;

    void check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::printf("FAIL: %s\n", message);
            std::exit(1);
        }
    }

    void reset_mocks()
    {
        current_thread_id = 1;
        in_irq = false;
        interrupt_state = 0;
        barrier_count = 0;
        block_count = 0;
        wake_all_count = 0;
        schedule_count = 0;
        wake_higher_priority = false;
        on_block = nullptr;
        blocked_guard = nullptr;
    }

    template<typename Function>
    void expect_fatal(Function function, const char* message)
    {
        const auto child = fork();
        check(child >= 0, "fork failed");
        if (child == 0)
        {
            function();
            _Exit(0);
        }

        int status = 0;
        check(waitpid(child, &status, 0) == child, "waitpid failed");
        check((WIFEXITED(status) && WEXITSTATUS(status) == fatal_exit) || WIFSIGNALED(status), message);
    }

    void finish_blocked_initialization()
    {
        const auto waiting_thread = current_thread_id;
        current_thread_id = 1;
        __cxa_guard_release(blocked_guard);
        current_thread_id = waiting_thread;
    }

    void test_acquire_release_and_fast_path()
    {
        reset_mocks();
        volatile std::uint32_t guard = 0;

        check(__cxa_guard_acquire(&guard) == 1, "first acquisition did not win initialization");
        check(guard == in_progress, "first acquisition did not mark the guard in progress");
        check(interrupt_state == 0, "acquisition did not restore interrupts");

        __cxa_guard_release(&guard);
        check(guard == initialized, "release did not mark the guard initialized");
        check(wake_all_count == 1, "release did not inspect the guard waiters");
        check(__cxa_guard_acquire(&guard) == 0, "initialized guard did not take the fast path");
    }

    void test_abort_allows_retry()
    {
        reset_mocks();
        volatile std::uint32_t guard = 0;

        check(__cxa_guard_acquire(&guard) == 1, "initial acquisition failed before abort");
        __cxa_guard_abort(&guard);
        check(guard == 0, "abort did not clear the guard");
        check(__cxa_guard_acquire(&guard) == 1, "aborted initialization could not be retried");
        __cxa_guard_release(&guard);
    }

    void test_contender_blocks_and_rechecks()
    {
        reset_mocks();
        volatile std::uint32_t guard = 0;

        check(__cxa_guard_acquire(&guard) == 1, "owner failed to acquire guard");
        current_thread_id = 2;
        blocked_guard = &guard;
        on_block = finish_blocked_initialization;
        wake_higher_priority = true;

        check(__cxa_guard_acquire(&guard) == 0, "contender did not recheck after being awakened");
        check(block_count == 1, "contender did not block exactly once");
        check(wake_all_count == 1, "release did not wake all contenders");
        check(schedule_count == 1, "higher-priority wake did not request scheduling");
        check(interrupt_state == 0, "contended acquisition did not restore interrupts");
    }

    void test_guard_slots_are_reused()
    {
        reset_mocks();
        volatile std::uint32_t first = 0;
        volatile std::uint32_t second = 0;
        volatile std::uint32_t third = 0;

        check(__cxa_guard_acquire(&first) == 1, "first guard acquisition failed");
        check(__cxa_guard_acquire(&second) == 1, "second guard acquisition failed");
        __cxa_guard_release(&first);
        check(__cxa_guard_acquire(&third) == 1, "released guard slot was not reused");
        __cxa_guard_release(&second);
        __cxa_guard_release(&third);
    }

    void test_recursive_initialization_traps()
    {
        reset_mocks();
        expect_fatal(
            [] {
                volatile std::uint32_t guard = 0;
                (void)__cxa_guard_acquire(&guard);
                (void)__cxa_guard_acquire(&guard);
            },
            "recursive initialization did not trap");
    }

    void test_irq_contention_traps()
    {
        reset_mocks();
        expect_fatal(
            [] {
                volatile std::uint32_t guard = 0;
                (void)__cxa_guard_acquire(&guard);
                current_thread_id = 2;
                in_irq = true;
                (void)__cxa_guard_acquire(&guard);
            },
            "IRQ contention did not trap");
    }

    void test_capacity_exhaustion_aborts()
    {
        reset_mocks();
        expect_fatal(
            [] {
                volatile std::uint32_t first = 0;
                volatile std::uint32_t second = 0;
                volatile std::uint32_t third = 0;
                (void)__cxa_guard_acquire(&first);
                (void)__cxa_guard_acquire(&second);
                (void)__cxa_guard_acquire(&third);
            },
            "active-guard capacity exhaustion did not abort");
    }

    struct test_case_t
    {
        const char* name;
        void (*run)();
    };

    const std::array cases {
        test_case_t { "acquire, release, and fast path", test_acquire_release_and_fast_path },
        test_case_t { "abort and retry", test_abort_allows_retry },
        test_case_t { "contended acquisition", test_contender_blocks_and_rechecks },
        test_case_t { "guard slot reuse", test_guard_slots_are_reused },
        test_case_t { "recursive initialization", test_recursive_initialization_traps },
        test_case_t { "IRQ contention", test_irq_contention_traps },
        test_case_t { "capacity exhaustion", test_capacity_exhaustion_aborts },
    };
}

namespace cpu::port {
    primask_t irq_save_disable() noexcept
    {
        const auto previous = interrupt_state;
        interrupt_state = 1;
        return previous;
    }

    void irq_restore(primask_t state) noexcept
    {
        interrupt_state = state;
    }

    bool is_in_irq() noexcept
    {
        return in_irq;
    }

    void data_memory_barrier() noexcept
    {
        ++barrier_count;
    }

    [[noreturn]] void fatal_trap() noexcept
    {
        _Exit(fatal_exit);
    }
}

namespace artos::thread {
    id_t get_current_thread() noexcept
    {
        return current_thread_id;
    }
}

namespace artos::kernel::locked {
    void block_current(wait_queue_t&, wait_forever_t) noexcept
    {
        ++block_count;
        check(on_block != nullptr, "block hook was not installed");
        on_block();
    }

    bool wake_all(wait_queue_t&, wake_reason_t) noexcept
    {
        ++wake_all_count;
        return wake_higher_priority;
    }
}

namespace artos::kernel::internal {
    void sched() noexcept
    {
        ++schedule_count;
    }
}

int main()
{
    for (const auto& test : cases)
    {
        test.run();
        std::printf("PASS: %s\n", test.name);
    }

    std::printf("%zu tests, 0 failed\n", cases.size());
    return 0;
}
