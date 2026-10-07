#include <board_services.h>
#include <build_config.h>

#include <internal/kernel_state.h>
#include <internal/thread.h>

#include <artos.h>
#include <artos_port.h>
#include <cpu_port.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace {
    using artos::kernel::internal::current_thread;
    using artos::kernel::internal::runtime_history_bucket_index;
    using artos::kernel::internal::threads;
    using artos::thread::internal::tcb_t;

    std::array<std::optional<tcb_t>, build_config::artos::max_threads>        tcbs {};
    std::array<artos::thread::stack_word_t, build_config::artos::max_threads> stacks {};
    std::uint32_t                                                             stopwatch_cycles = 0;
    unsigned                                                                  mask_count = 0;
    unsigned                                                                  restore_count = 0;
    cpu::port::basepri_t                                                      last_mask = 0;
    cpu::port::basepri_t                                                      last_restored = 0;

    void check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::printf("FAIL: %s\n", message);
            std::exit(1);
        }
    }

    void reset()
    {
        threads.fill(nullptr);
        for (auto& tcb : tcbs)
        {
            tcb.reset();
        }
        current_thread = nullptr;
        runtime_history_bucket_index = 0;
        stopwatch_cycles = 0;
        mask_count = 0;
        restore_count = 0;
        last_mask = 0;
        last_restored = 0;
    }

    tcb_t& install(std::size_t slot, std::string_view name, artos::thread::state_t state)
    {
        auto& thread = tcbs[slot].emplace(name, artos::thread::priority_t::low, &stacks[slot], &stacks[slot], 1);
        thread.make_thread_id(slot);
        thread.state = state;
        threads[slot] = &thread;
        return thread;
    }

    void check_mask_restored()
    {
        check(mask_count == 1 && restore_count == 1, "snapshot did not use one critical section");
        check(last_mask == artos::port::kernel_interrupt_priority, "snapshot used the wrong interrupt mask");
        check(last_restored == 0x20, "snapshot did not restore the previous interrupt mask");
    }

    void test_empty_registry()
    {
        reset();
        artos::thread::snapshot_array_t snapshot {};
        snapshot[0].id = 123;
        check(artos::thread::get_snapshot(snapshot) == 0, "empty registry returned entries");
        check(snapshot[0].id == 123, "empty snapshot modified caller storage");
        check_mask_restored();
    }

    void test_sparse_registry_and_owned_names()
    {
        reset();
        auto&                 idle = install(0, "IDLE", artos::thread::state_t::ready);
        auto&                 worker = install(3, "worker", artos::thread::state_t::running);
        artos::thread::name_t long_name {};
        long_name.fill('x');
        long_name.back() = '\0';
        auto& last = install(threads.size() - 1,
                             std::string_view { long_name.data(), build_config::artos::max_thread_name_length },
                             artos::thread::state_t::blocked);
        worker.priority = artos::thread::priority_t::high;
        current_thread = &worker;

#if defined(ARTOS_TEST_THREAD_STATS)
        stopwatch_cycles = 100;
        worker.pre_run();
        runtime_history_bucket_index = 2;
        worker.statistics.runtime_history_cycles[2] = 8;
        idle.statistics.execution_count = 4;
        last.statistics.runtime_history_cycles[1] = 17;
        stopwatch_cycles = 125;
#endif

        artos::thread::snapshot_array_t snapshot {};
        snapshot[3].id = 123;
#if !defined(ARTOS_TEST_THREAD_STATS)
        snapshot[0].statistics.execution_count = 99;
        snapshot[1].statistics.runtime_history_cycles[2] = 99;
#endif
        check(artos::thread::get_snapshot(snapshot) == 3, "sparse registry returned the wrong count");
        check(snapshot[0].id == idle.id && snapshot[1].id == worker.id && snapshot[2].id == last.id,
              "snapshot did not compact entries in slot order");
        check(std::string_view { snapshot[0].name.data() } == "IDLE"
                  && std::string_view { snapshot[1].name.data() } == "worker",
              "snapshot copied names incorrectly");
        check(snapshot[2].name[build_config::artos::max_thread_name_length - 1] == 'x'
                  && snapshot[2].name.back() == '\0',
              "maximum-length snapshot name was not copied and terminated");
        check(snapshot[1].state == artos::thread::state_t::running
                  && snapshot[1].priority == artos::thread::priority_t::high,
              "snapshot copied the wrong state or priority");
        check(snapshot[3].id == 123, "snapshot wrote past its returned count");
        check_mask_restored();

        worker.name[0] = 'W';
        check(snapshot[1].name[0] == 'w', "snapshot name still refers to the TCB");

#if defined(ARTOS_TEST_THREAD_STATS)
        check(snapshot[0].statistics.execution_count == 4, "idle execution count was not copied");
        check(snapshot[1].statistics.execution_count == 1, "running execution count was not copied");
        check(snapshot[1].statistics.runtime_history_cycles[2] == 33, "running thread elapsed cycles were omitted");
        check(snapshot[2].statistics.runtime_history_cycles[1] == 17, "blocked thread history was not copied");
        check(worker.statistics.runtime_history_cycles[2] == 8,
              "snapshot changed the running thread's accumulated cycles");
        stopwatch_cycles = 130;
        check(artos::thread::get_snapshot(snapshot) == 3, "second snapshot returned the wrong count");
        check(snapshot[1].statistics.runtime_history_cycles[2] == 38, "second snapshot double-counted running cycles");
#else
        check(snapshot[0].statistics.execution_count == 0 && snapshot[1].statistics.runtime_history_cycles[2] == 0,
              "statistics-disabled snapshot returned nonzero statistics");
#endif
    }

    void test_full_registry()
    {
        reset();
        for (std::size_t slot = 0; slot < threads.size(); ++slot)
        {
            install(slot, "full", artos::thread::state_t::ready);
        }
        artos::thread::snapshot_array_t snapshot {};
        check(artos::thread::get_snapshot(snapshot) == threads.size(), "full registry was truncated");
        for (std::size_t slot = 0; slot < threads.size(); ++slot)
        {
            check(snapshot[slot].id == tcbs[slot]->id, "full snapshot changed slot order");
        }
        check_mask_restored();
    }
}

namespace cpu::port {
    basepri_t irq_save_raise_mask(basepri_t priority) noexcept
    {
        ++mask_count;
        last_mask = priority;
        return 0x20;
    }

    void irq_restore_mask(basepri_t previous) noexcept
    {
        ++restore_count;
        last_restored = previous;
    }
}

std::uint32_t board::stopwatch::count() noexcept
{
    return stopwatch_cycles;
}

int main()
{
    test_empty_registry();
    test_sparse_registry_and_owned_names();
    test_full_registry();
    std::puts("PASS: thread snapshot");
}
