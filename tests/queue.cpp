#include <queue.h>

#include <array>
#include <cstdio>
#include <stdexcept>

namespace {
    artos::wake_reason_t next_wait_result = artos::wake_reason_t::timeout;
    void (*on_block)() = nullptr;
    unsigned cancelled_wake_calls = 0;
}

namespace cpu::port {
    bool is_in_irq() noexcept
    {
        return false;
    }

    basepri_t irq_save_raise_mask(basepri_t priority) noexcept
    {
        return priority;
    }

    void irq_restore_mask(basepri_t) noexcept
    {
    }
}

namespace artos::kernel {
    tick_time_t tick_time() noexcept
    {
        return {};
    }

    namespace locked {
        tick_time_t tick_time() noexcept
        {
            return {};
        }

        wake_reason_t block_current(wait_queue_t&, const tick_time_t&, critical_section_t) noexcept
        {
            if (on_block)
            {
                on_block();
            }
            return next_wait_result;
        }

        wake_reason_t block_current(wait_queue_t&, wait_forever_t, critical_section_t) noexcept
        {
            return wake_reason_t::signaled;
        }

        bool wake_all(wait_queue_t&, wake_reason_t reason) noexcept
        {
            if (reason == wake_reason_t::cancelled)
            {
                ++cancelled_wake_calls;
            }
            return false;
        }

        bool wake_one(wait_queue_t&) noexcept
        {
            return false;
        }
    }

    namespace internal {
        thread::internal::tcb_base_t* current_thread {};

        void sched() noexcept
        {
        }
    }
}

namespace {
    void check(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    template<typename Type, std::size_t Count>
    concept valid_queue_storage = requires { typename artos::queue_storage_t<Type, Count>; };

    static_assert(valid_queue_storage<int, 1>);
    static_assert(!valid_queue_storage<int, 0>);
    static_assert(!valid_queue_storage<const int, 1>);
    static_assert(!valid_queue_storage<int&, 1>);

    void test_empty_queue()
    {
        artos::queue_storage_t<int, 2> storage;
        artos::queue_t<int>            queue { storage };
        int                            output = 42;

        check(queue.pop_front(output, 0) == artos::queue_result_t::timeout, "pop_front succeeded on an empty queue");
        check(output == 42, "failed pop_front modified its output");
        check(queue.pop_back(output, 0) == artos::queue_result_t::timeout, "pop_back succeeded on an empty queue");
        check(output == 42, "failed pop_back modified its output");
    }

    void test_fifo_and_wraparound()
    {
        artos::queue_storage_t<int, 3> storage;
        artos::queue_t<int>            queue { storage };
        int                            output {};

        check(queue.push_back(1, 0) == artos::queue_result_t::success, "first push_back failed");
        check(queue.push_back(2, 0) == artos::queue_result_t::success, "second push_back failed");
        check(queue.pop_front(output, 0) == artos::queue_result_t::success && output == 1,
              "FIFO returned the wrong first item");
        check(queue.push_back(3, 0) == artos::queue_result_t::success, "push_back before wraparound failed");
        check(queue.push_back(4, 0) == artos::queue_result_t::success, "wrapped push_back failed");
        check(queue.pop_front(output, 0) == artos::queue_result_t::success && output == 2,
              "FIFO returned the wrong second item");
        check(queue.pop_front(output, 0) == artos::queue_result_t::success && output == 3,
              "FIFO returned the wrong third item");
        check(queue.pop_front(output, 0) == artos::queue_result_t::success && output == 4,
              "FIFO returned the wrong wrapped item");
    }

    void test_front_and_back_operations()
    {
        artos::queue_storage_t<int, 4> storage;
        artos::queue_t<int>            queue { storage };
        int                            output {};

        check(queue.push_back(2, 0) == artos::queue_result_t::success, "push_back failed");
        check(queue.push_front(1, 0) == artos::queue_result_t::success, "push_front failed");
        check(queue.push_back(3, 0) == artos::queue_result_t::success, "second push_back failed");
        check(queue.push_front(0, 0) == artos::queue_result_t::success, "second push_front failed");
        check(queue.pop_front(output, 0) == artos::queue_result_t::success && output == 0,
              "pop_front returned the wrong item");
        check(queue.pop_back(output, 0) == artos::queue_result_t::success && output == 3,
              "pop_back returned the wrong item");
        check(queue.pop_front(output, 0) == artos::queue_result_t::success && output == 1,
              "remaining front item was wrong");
        check(queue.pop_back(output, 0) == artos::queue_result_t::success && output == 2,
              "remaining back item was wrong");
    }

    void test_full_queue_preserves_contents()
    {
        artos::queue_storage_t<int, 2> storage;
        artos::queue_t<int>            queue { storage };
        int                            output {};

        check(queue.push_back(10, 0) == artos::queue_result_t::success, "first push failed");
        check(queue.push_back(20, 0) == artos::queue_result_t::success, "second push failed");
        check(queue.push_back(30, 0) == artos::queue_result_t::timeout, "push_back succeeded on a full queue");
        check(queue.push_front(0, 0) == artos::queue_result_t::timeout, "push_front succeeded on a full queue");
        check(queue.pop_front(output, 0) == artos::queue_result_t::success && output == 10,
              "failed push changed the first item");
        check(queue.pop_front(output, 0) == artos::queue_result_t::success && output == 20,
              "failed push changed the second item");
    }

    void test_timed_wait_reports_timeout()
    {
        artos::queue_storage_t<int, 1> storage;
        artos::queue_t<int>            queue { storage };
        int                            output = 42;

        check(queue.pop_front(output, 10) == artos::queue_result_t::timeout, "timed reader did not time out");
        check(output == 42, "timed reader changed its output");
        check(queue.push_back(1, 0) == artos::queue_result_t::success, "writer setup failed");
        check(queue.push_back(2, 10) == artos::queue_result_t::timeout, "timed writer did not time out");
    }

    void test_close_is_permanent()
    {
        artos::queue_storage_t<int, 2> storage;
        artos::queue_t<int>            queue { storage };
        int                            output = 42;
        const auto                     wakes_before = cancelled_wake_calls;

        check(queue.push_back(7, 0) == artos::queue_result_t::success, "initial push failed");
        queue.close();
        check(cancelled_wake_calls == wakes_before + 2, "close did not cancel both wait queues");
        queue.close();
        check(cancelled_wake_calls == wakes_before + 2, "second close cancelled again");
        check(queue.push_front(1, 0) == artos::queue_result_t::closed, "push_front after close was accepted");
        check(queue.push_back(1, 0) == artos::queue_result_t::closed, "push_back after close was accepted");
        check(queue.pop_front(output, 0) == artos::queue_result_t::closed, "pop_front after close was accepted");
        check(queue.pop_back(output, 0) == artos::queue_result_t::closed, "pop_back after close was accepted");
        check(output == 42, "closed pop changed its output");
        queue.close_and_wait();
    }

    artos::queue_t<int>* queue_to_close = nullptr;

    void close_during_wait()
    {
        queue_to_close->close();
    }

    void test_blocked_operations_report_close()
    {
        artos::queue_storage_t<int, 1> storage;
        artos::queue_t<int>            reader { storage };
        int                            output = 42;
        queue_to_close = &reader;
        on_block = close_during_wait;
        next_wait_result = artos::wake_reason_t::cancelled;
        check(reader.pop_front(output, 10) == artos::queue_result_t::closed, "cancelled reader reported wrong result");
        check(output == 42, "cancelled reader changed its output");
        reader.close_and_wait();

        artos::queue_t<int> writer { storage };
        check(writer.push_back(1, 0) == artos::queue_result_t::success, "writer setup failed");
        queue_to_close = &writer;
        check(writer.push_back(2, 10) == artos::queue_result_t::closed, "cancelled writer reported wrong result");
        writer.close_and_wait();
        on_block = nullptr;
        queue_to_close = nullptr;
        next_wait_result = artos::wake_reason_t::timeout;
    }

    struct message_t
    {
        std::uint32_t       id;
        std::array<char, 4> payload;

        bool operator==(const message_t&) const = default;
    };

    void test_trivially_copyable_value()
    {
        artos::queue_storage_t<message_t, 1> storage;
        artos::queue_t<message_t>            queue { storage };
        const message_t                      input { 7, { 'a', 'r', 't', 's' } };
        message_t                            output {};

        check(queue.push_back(input, 0) == artos::queue_result_t::success, "struct push failed");
        check(queue.pop_front(output, 0) == artos::queue_result_t::success, "struct pop failed");
        check(output == input, "struct value was not copied intact");
    }

    struct test_case_t
    {
        const char* name;
        void (*run)();
    };

    const std::array cases {
        test_case_t { "empty queue", test_empty_queue },
        test_case_t { "FIFO and wraparound", test_fifo_and_wraparound },
        test_case_t { "front and back operations", test_front_and_back_operations },
        test_case_t { "full queue", test_full_queue_preserves_contents },
        test_case_t { "trivially copyable value", test_trivially_copyable_value },
        test_case_t { "timed waits", test_timed_wait_reports_timeout },
        test_case_t { "close is permanent", test_close_is_permanent },
        test_case_t { "blocked operations report close", test_blocked_operations_report_close },
    };
}

int main()
{
    unsigned failed = 0;
    for (const auto& test : cases)
    {
        try
        {
            test.run();
            std::printf("PASS: %s\n", test.name);
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::printf("FAIL: %s: %s\n", test.name, error.what());
        }
    }

    std::printf("%zu tests, %u failed\n", cases.size(), failed);
    return failed == 0 ? 0 : 1;
}
