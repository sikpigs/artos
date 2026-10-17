#include <array>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>

#include <artos.h>
#include <debug_test_suite.h>
#include <event.h>

#include <internal/kernel_state.h>
#include <internal/scheduler.h>

namespace {
    using namespace artos;
    using host_tests::check;

    bool                 interrupt_context {};
    unsigned             scheduling_requests {};
    cpu::port::basepri_t interrupt_mask {};

    struct fixture_t
    {
        fixture_t()
        {
            interrupt_context = false;
            scheduling_requests = 0;
            interrupt_mask = 0;
        }

        ~fixture_t()
        {
            interrupt_context = false;
        }
    };

    void value(event_result_t result, bits_t expected, const char* message)
    {
        check(result.has_value() && *result == expected, message);
    }

    void error(event_result_t result, event_error_t expected, const char* message)
    {
        check(!result.has_value() && result.error() == expected, message);
    }

    // Characterization of empty masks; document this deliberately if retained.
    void test_zero_mask_semantics()
    {
        const fixture_t fixture;
        event_t         event { 3 };
        error(event.wait_for(any_t { 0 }, true, tick_t { 0 }),
              event_error_t::timeout,
              "empty any-mask unexpectedly matched");
        value(event.wait_for(all_t { 0 }, true, tick_t { 0 }), 3, "empty all-mask did not match vacuously");
        value(event.set(0), 3, "empty reset mask changed event bits");
        value(event.wait_for(all_t { 0 }, false, wait_forever), 0, "empty all-mask failed on an empty bitset");
    }

    void test_constructed_event_is_open()
    {
        const fixture_t fixture;
        event_t         event;
        error(event.wait_for(any_t { 1 }, false, tick_t { 0 }),
              event_error_t::timeout,
              "empty open event must time out, not report closed");
        value(event.post(1), 0, "post must return the previous bitset");
        value(event.wait_for(any_t { 1 }, false, tick_t { 0 }), 1, "posted bit was not available");
    }

    void test_wait_all_matches_requested_bits()
    {
        const fixture_t fixture;
        event_t         event { 0b011 };
        value(event.wait_for(all_t { 0b001 }, false, tick_t { 0 }),
              0b011,
              "wait-all rejected a present subset or returned the wrong snapshot");
    }

    void test_wait_all_rejects_missing_requested_bits()
    {
        const fixture_t fixture;
        event_t         event { 0b001 };
        error(event.wait_for(all_t { 0b011 }, false, tick_t { 0 }),
              event_error_t::timeout,
              "wait-all succeeded while a requested bit was absent");
        value(event.wait_for(any_t { 1 }, false, tick_t { 0 }), 1, "timeout changed existing bits");
    }

    void test_wait_clear_removes_received_bits()
    {
        const fixture_t fixture;
        event_t         event { 0b111 };
        value(event.wait_for(any_t { 0b011 }, true, tick_t { 0 }),
              0b111,
              "resetting wait must return the snapshot before clearing");
        error(event.wait_for(any_t { 0b011 }, false, tick_t { 0 }),
              event_error_t::timeout,
              "resetting wait did not clear the received bits");
        value(event.wait_for(any_t { 0b100 }, false, tick_t { 0 }), 0b100, "resetting wait cleared unrelated bits");
    }

    void test_wait_all_reset_and_forever_overloads()
    {
        const fixture_t fixture;
        event_t         event { 0b111 };
        value(event.wait_for(all_t { 0b011 }, true, wait_forever),
              0b111,
              "wait-all forever did not return the pre-reset snapshot");
        value(
            event.wait_for(any_t { 0b100 }, false, wait_forever), 0b100, "wait-any forever failed on an available bit");
    }

    void test_nonresetting_wait_preserves_bits()
    {
        const fixture_t fixture;
        event_t         event { 0b101 };
        value(event.wait_for(any_t { 1 }, false, tick_t { 0 }), 0b101, "first wait snapshot was wrong");
        value(event.wait_for(any_t { 1 }, false, tick_t { 0 }), 0b101, "non-resetting wait consumed bits");
    }

    void test_set_and_post_semantics()
    {
        const fixture_t fixture;
        event_t         event { 0b001 };
        value(event.post(0b100), 0b001, "post did not return old state");
        value(event.set(0b010), 0b101, "post did not merge bits or set returned wrong old state");
        value(event.set(0), 0b010, "set did not replace the whole bitset");
        error(event.wait_for(any_t { 0b111 }, false, tick_t { 0 }),
              event_error_t::timeout,
              "set(0) did not clear the bitset");
    }

    void test_full_width_bits()
    {
        const fixture_t fixture;
        constexpr auto  high = bits_t { 1 } << (std::numeric_limits<bits_t>::digits - 1);
        event_t         event;
        value(event.post(high), 0, "highest bit was rejected");
        value(event.wait_for(all_t { high }, true, tick_t { 0 }), high, "highest bit was not delivered");
        constexpr auto full = std::numeric_limits<bits_t>::max();
        value(event.set(full), 0, "highest bit was not reset");
        value(event.wait_for(all_t { full }, true, tick_t { 0 }), full, "full bitset was not delivered");
    }

    void test_thread_post_rejects_irq()
    {
        const fixture_t fixture;
        event_t         event { 1 };
        interrupt_context = true;
        error(event.post(2), event_error_t::unsupported, "thread post accepted ISR context");
        interrupt_context = false;
        value(event.set(0), 1, "rejected ISR post modified event state");
    }

    void test_thread_set_rejects_irq()
    {
        const fixture_t fixture;
        event_t         event { 1 };
        interrupt_context = true;
        error(event.set(2), event_error_t::unsupported, "thread set accepted ISR context");
        interrupt_context = false;
        value(event.set(0), 1, "rejected ISR set modified event state");
    }

    void test_wait_rejects_irq()
    {
        const fixture_t fixture;
        event_t         event { 1 };
        interrupt_context = true;
        error(event.wait_for(any_t { 1 }, true, tick_t { 0 }),
              event_error_t::unsupported,
              "timed any wait accepted ISR context");
        error(event.wait_for(all_t { 1 }, true, tick_t { 0 }),
              event_error_t::unsupported,
              "timed all wait accepted ISR context");
        error(event.wait_for(any_t { 1 }, true, wait_forever),
              event_error_t::unsupported,
              "forever any wait accepted ISR context");
        error(event.wait_for(all_t { 1 }, true, wait_forever),
              event_error_t::unsupported,
              "forever all wait accepted ISR context");
        interrupt_context = false;
        value(event.set(0), 1, "rejected ISR wait consumed bits");
    }

    template<bool Replace>
    void test_isr_updates()
    {
        const fixture_t fixture;
        event_t         event { 1 };
        interrupt_context = true;
        bool       woken = false;
        const auto result = Replace ? event.set_from_isr(2, &woken) : event.post_from_isr(2, &woken);
        value(result, 1, "ISR update did not return previous state");
        check(!woken, "ISR update without waiters set the wake flag");
        woken = true;
        value(Replace ? event.set_from_isr(2, &woken) : event.post_from_isr(2, &woken),
              Replace ? 2 : 3,
              "repeated ISR update returned wrong previous state");
        check(woken, "ISR update cleared an accumulated wake flag");
        (void)(Replace ? event.set_from_isr(2, nullptr) : event.post_from_isr(2, nullptr));
        (void)(Replace ? event.set_from_isr(2) : event.post_from_isr(2));
        check(scheduling_requests == 0, "ISR update requested scheduling internally");
        interrupt_context = false;
        value(event.set(0), Replace ? 2 : 3, "ISR update did not preserve OR/replace semantics");
    }

    void test_close_rejects_all_operations()
    {
        const fixture_t fixture;
        event_t         event { 1 };
        event.close();
        event.close();
        error(event.post(2), event_error_t::closed, "closed event accepted post");
        error(event.set(2), event_error_t::closed, "closed event accepted set");
        error(
            event.wait_for(any_t { 1 }, false, tick_t { 0 }), event_error_t::closed, "closed event accepted wait-any");
        error(
            event.wait_for(all_t { 1 }, false, wait_forever), event_error_t::closed, "closed event accepted wait-all");
        interrupt_context = true;
        bool woken = true;
        error(event.post_from_isr(2, &woken), event_error_t::closed, "closed event accepted ISR post");
        check(woken, "closed ISR post cleared an accumulated wake flag");
        woken = false;
        error(event.set_from_isr(2, &woken), event_error_t::closed, "closed event accepted ISR set");
        check(!woken, "closed ISR set reported a wake");
        interrupt_context = false;
        event.close_and_wait();
    }

    void test_identity_cannot_be_copied_or_moved()
    {
        check(!std::is_copy_constructible_v<event_t>, "event_t is copyable despite owning intrusive wait queues");
        check(!std::is_copy_assignable_v<event_t>, "event_t permits copy assignment");
        check(!std::is_move_constructible_v<event_t>, "event_t can move while threads reference its address");
        check(!std::is_move_assignable_v<event_t>, "event_t permits move assignment");
    }
}

namespace cpu::port {
    basepri_t irq_save_raise_mask(basepri_t priority) noexcept
    {
        return std::exchange(interrupt_mask, priority);
    }

    void irq_restore_mask(basepri_t previous) noexcept
    {
        interrupt_mask = previous;
    }

    bool is_in_irq() noexcept
    {
        return interrupt_context;
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

        bool wake_all(wait_queue_t&, wake_reason_t) noexcept
        {
            return false;
        }

        bool wake_one(wait_queue_t&) noexcept
        {
            return false;
        }

        wake_reason_t block_current(wait_queue_t&, const tick_time_t&, critical_section_t) noexcept
        {
            std::abort();
        }

        wake_reason_t block_current(wait_queue_t&, wait_forever_t, critical_section_t) noexcept
        {
            std::abort();
        }
    }

    namespace internal {
        void sched() noexcept
        {
            ++scheduling_requests;
        }

        thread::internal::tcb_base_t* current_thread {};
    }
}

int main(int argc, char** argv)
{
    const std::array cases {
        host_tests::test_case_t { "zero mask characterization", test_zero_mask_semantics },
        host_tests::test_case_t { "constructed event is open", test_constructed_event_is_open },
        host_tests::test_case_t { "wait-all accepts present requested subset", test_wait_all_matches_requested_bits },
        host_tests::test_case_t { "wait-all rejects absent requested bits",
                                  test_wait_all_rejects_missing_requested_bits },
        host_tests::test_case_t { "wait clear removes received bits", test_wait_clear_removes_received_bits },
        host_tests::test_case_t { "wait-all reset and forever overloads", test_wait_all_reset_and_forever_overloads },
        host_tests::test_case_t { "non-resetting wait preserves bits", test_nonresetting_wait_preserves_bits },
        host_tests::test_case_t { "set, post, and clear semantics", test_set_and_post_semantics },
        host_tests::test_case_t { "full-width bitset", test_full_width_bits },
        host_tests::test_case_t { "thread post rejects ISR", test_thread_post_rejects_irq },
        host_tests::test_case_t { "thread set rejects ISR", test_thread_set_rejects_irq },
        host_tests::test_case_t { "all wait overloads reject ISR", test_wait_rejects_irq },
        host_tests::test_case_t { "ISR post state and flags", test_isr_updates<false> },
        host_tests::test_case_t { "ISR set state and flags", test_isr_updates<true> },
        host_tests::test_case_t { "close rejects all operations", test_close_rejects_all_operations },
        host_tests::test_case_t { "event identity cannot be copied or moved", test_identity_cannot_be_copied_or_moved },
    };
    return host_tests::run_cases(argc, argv, cases);
}
