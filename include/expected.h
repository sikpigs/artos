#pragma once

#include <type_traits>

namespace artos {
    // Contract: integral/enum payloads only, no allocation or exceptions.
    // Deliberately excludes void, owning objects, value(), and monadic methods.
    template<typename E>
    struct unexpected
    {
        E error_value;
    };

    template<typename E>
    unexpected(E) -> unexpected<E>;

    template<typename T, typename E>
    class expected
    {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
        static_assert(std::is_integral_v<E> || std::is_enum_v<E>);

    public:
        using value_type = T;
        using error_type = E;

        constexpr expected() noexcept
            : context {}
            , contains_value { true }
        {
        }

        constexpr expected(T value) noexcept
            : context { .value = value }
            , contains_value { true }
        {
        }

        constexpr expected(unexpected<E> failure) noexcept
            : context { .error = failure.error_value }
            , contains_value { false }
        {
        }

        // Scalar members allow ordinary memberwise copying and moving.
        expected(const expected&) noexcept = default;
        expected(expected&&) noexcept = default;
        expected& operator=(const expected&) noexcept = default;
        expected& operator=(expected&&) noexcept = default;

        [[nodiscard]] constexpr bool has_value() const noexcept
        {
            return contains_value;
        }

        [[nodiscard]] explicit constexpr operator bool() const noexcept
        {
            return has_value();
        }

        // Preconditions: dereference only a success, error() only a failure.
        // Wrong-state access is outside this small contract; no throwing value().
        constexpr T& operator*() noexcept
        {
            return context.value;
        }

        constexpr const T& operator*() const noexcept
        {
            return context.value;
        }

        constexpr E& error() noexcept
        {
            return context.error;
        }

        constexpr const E& error() const noexcept
        {
            return context.error;
        }

    private:
        union
        {
            T value;
            E error;
        } context;
        bool contains_value { false };
    };
}
