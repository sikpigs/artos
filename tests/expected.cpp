#if defined(ARTOS_TEST_AUTO_EXPECTED)
#include <expected_compat.h>
namespace subject = artos;
#elif defined(ARTOS_TEST_STD_EXPECTED)
#include <expected>
namespace subject = std;
#else
#include <expected.h>
namespace subject = artos;
#endif

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
    using result_t = subject::expected<std::uintptr_t, int>;

    static_assert(!std::is_convertible_v<result_t, bool>, "bool conversion must be explicit");
    static_assert(std::is_nothrow_copy_constructible_v<result_t>);
    static_assert(std::is_nothrow_move_constructible_v<result_t>);
    static_assert(std::is_nothrow_copy_assignable_v<result_t>);
    static_assert(std::is_nothrow_move_assignable_v<result_t>);
    static_assert(std::is_same_v<decltype(*std::declval<result_t&>()), std::uintptr_t&>);
    static_assert(std::is_same_v<decltype(*std::declval<const result_t&>()), const std::uintptr_t&>);
    static_assert(std::is_same_v<decltype(std::declval<result_t&>().error()), int&>);
    static_assert(std::is_same_v<decltype(std::declval<const result_t&>().error()), const int&>);

    void check(bool condition, const char* message)
    {
        // Active in Release builds too. The tested header itself needs no exceptions.
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void success(const result_t& result, std::uintptr_t value)
    {
        check(result.has_value(), "success must have a value");
        check(static_cast<bool>(result), "success must convert to true");
        check(*result == value, "successful value was not preserved");
    }

    void failure(const result_t& result, int error)
    {
        check(!result.has_value(), "failure must not have a value");
        check(!result, "failure must convert to false");
        check(result.error() == error, "error was not preserved");
    }

    result_t create_like(bool fail)
    {
        if (fail)
        {
            return subject::unexpected(EAGAIN);
        }
        return std::uintptr_t { 7 };
    }

    struct test_case
    {
        const char* name;
        void (*run)();
    };

    const std::array cases {
        test_case { "default success", [] { success(result_t {}, 0); } },
        test_case { "zero is a successful thread ID", [] { success(result_t { std::uintptr_t { 0 } }, 0); } },
        test_case { "successful ID", [] { success(result_t { std::uintptr_t { 42 } }, 42); } },
        test_case { "error construction", [] { failure(result_t { subject::unexpected(EEXIST) }, EEXIST); } },
        test_case { "zero error still means failure", [] { failure(result_t { subject::unexpected(0) }, 0); } },
        test_case { "implicit return conversions",
                    [] {
                        success(create_like(false), 7);
                        failure(create_like(true), EAGAIN);
                    } },
        test_case { "mutable accessors",
                    [] {
                        result_t value { std::uintptr_t { 1 } };
                        success(value, 1);
                        *value = 9;
                        success(value, 9);
                        result_t error { subject::unexpected(EAGAIN) };
                        failure(error, EAGAIN);
                        error.error() = EEXIST;
                        failure(error, EEXIST);
                    } },
        test_case { "copy and move construction",
                    [] {
                        const result_t value { std::uintptr_t { 23 } };
                        const result_t error { subject::unexpected(EAGAIN) };
                        auto value_copy = value;
                        auto error_copy = error;
                        success(value_copy, 23);
                        failure(error_copy, EAGAIN);
                        success(result_t { std::move(value_copy) }, 23);
                        failure(result_t { std::move(error_copy) }, EAGAIN);
                        success(value, 23);
                        failure(error, EAGAIN);
                    } },
        test_case { "copy assignment changes state both ways",
                    [] {
                        result_t value { std::uintptr_t { 3 } };
                        const result_t error { subject::unexpected(EEXIST) };
                        value = error;
                        failure(value, EEXIST);
                        const result_t other { std::uintptr_t { 5 } };
                        value = other;
                        success(value, 5);
                    } },
        test_case { "move assignment changes state both ways",
                    [] {
                        result_t result { std::uintptr_t { 3 } };
                        result = result_t { subject::unexpected(EAGAIN) };
                        failure(result, EAGAIN);
                        result = result_t { std::uintptr_t { 5 } };
                        success(result, 5);
                    } },
        test_case { "same value and error types stay distinguishable",
                    [] {
                        const subject::expected<int, int> value { 11 };
                        const subject::expected<int, int> error { subject::unexpected(11) };
                        check(value.has_value() && !error.has_value(), "state cannot depend on payload type");
                        check(*value == 11 && error.error() == 11, "same-type payload lost");
                    } },
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
