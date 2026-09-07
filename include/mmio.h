#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

template<typename Tag, std::uint32_t Mask>
struct field_value_t
{
    static constexpr std::uint32_t mask = Mask;
    std::uint32_t                  encoded_value;
};

template<std::uint32_t... Masks>
consteval bool masks_are_disjoint()
{
    std::uint32_t used = 0;
    bool          disjoint = true;
    ((disjoint = disjoint && (used & Masks) == 0, used |= Masks), ...);
    return disjoint;
}

template<typename Tag, std::uint32_t Position, std::uint32_t Width = 1>
struct field_t
{
    static_assert(Width > 0 && Position + Width <= 32);
    static constexpr std::uint32_t value_mask = Width == 32 ? -std::uint32_t {} : (std::uint32_t { 1 } << Width) - 1;
    static constexpr std::uint32_t mask = value_mask << Position;
    static constexpr std::uint32_t encode(std::uint32_t value)
    {
        return (value & value_mask) << Position;
    }

    static constexpr std::uint32_t decode(std::uint32_t value)
    {
        return (value >> Position) & value_mask;
    }

    [[nodiscard]] constexpr field_value_t<Tag, mask> operator()(std::uint32_t value) const noexcept
    {
        return { encode(value) };
    }
};

template<typename Tag>
struct register_t
{
    volatile std::uint32_t value;

    [[nodiscard]] std::uint32_t read() const noexcept
    {
        return value;
    }

    void write(std::uint32_t new_value) noexcept
    {
        value = new_value;
    }

    template<std::uint32_t Position, std::uint32_t Width>
    [[nodiscard]] std::uint32_t read(field_t<Tag, Position, Width>) const noexcept
    {
        return field_t<Tag, Position, Width>::decode(value);
    }

    template<std::uint32_t... Positions>
    void set(field_t<Tag, Positions>...) noexcept
    requires(sizeof...(Positions) > 0)
    {
        constexpr auto combined_mask = (std::uint32_t {} | ... | field_t<Tag, Positions>::mask);
        value = value | combined_mask;
    }

    template<std::uint32_t... Positions>
    void clear(field_t<Tag, Positions>...) noexcept
    requires(sizeof...(Positions) > 0)
    {
        constexpr auto combined_mask = (std::uint32_t {} | ... | field_t<Tag, Positions>::mask);
        value = value & ~combined_mask;
    }

    template<std::uint32_t Position, std::uint32_t Width>
    void write(field_t<Tag, Position, Width>, std::uint32_t new_value) noexcept
    {
        using field = field_t<Tag, Position, Width>;
        auto current = value;
        current = (current & ~field::mask) | field::encode(new_value);
        value = current;
    }

    template<std::uint32_t... Masks>
    void write(field_value_t<Tag, Masks>... new_values) noexcept
    requires(sizeof...(Masks) > 0)
    {
        static_assert(masks_are_disjoint<Masks...>(), "bulk-written register fields must not overlap");
        constexpr auto combined_mask = (std::uint32_t {} | ... | Masks);
        const auto     combined_value = (std::uint32_t {} | ... | new_values.encoded_value);
        const auto     current = value;
        value = (current & ~combined_mask) | combined_value;
    }
};

template<typename Tag>
struct write_only_register_t
{
    volatile std::uint32_t value;

    void write(std::uint32_t new_value) noexcept
    {
        value = new_value;
    }

    template<std::uint32_t Position, std::uint32_t Width>
    void write(field_t<Tag, Position, Width>, std::uint32_t new_value) noexcept
    {
        value = field_t<Tag, Position, Width>::encode(new_value);
    }

    template<std::uint32_t... Positions>
    void set(field_t<Tag, Positions>...) noexcept
    requires(sizeof...(Positions) > 0)
    {
        value = (std::uint32_t {} | ... | field_t<Tag, Positions>::mask);
    }

    template<std::uint32_t... Masks>
    void write(field_value_t<Tag, Masks>... new_values) noexcept
    requires(sizeof...(Masks) > 0)
    {
        static_assert(masks_are_disjoint<Masks...>(), "bulk-written register fields must not overlap");
        value = (std::uint32_t {} | ... | new_values.encoded_value);
    }
};

template<typename Tag>
struct read_only_register_t
{
    volatile const std::uint32_t value;

    [[nodiscard]] std::uint32_t read() const noexcept
    {
        return value;
    }

    template<std::uint32_t Position, std::uint32_t Width>
    [[nodiscard]] std::uint32_t read(field_t<Tag, Position, Width>) const noexcept
    {
        return field_t<Tag, Position, Width>::decode(value);
    }
};

template<typename Tag>
struct byte_register_t
{
    volatile std::uint8_t value;

    [[nodiscard]] std::uint8_t read() const noexcept
    {
        return value;
    }

    void write(std::uint8_t new_value) noexcept
    {
        value = new_value;
    }

    template<std::uint32_t Position, std::uint32_t Width>
    [[nodiscard]] std::uint32_t read(field_t<Tag, Position, Width>) const noexcept
    {
        return field_t<Tag, Position, Width>::decode(value);
    }

    template<std::uint32_t Position, std::uint32_t Width>
    void write(field_t<Tag, Position, Width>, std::uint32_t new_value) noexcept
    {
        using field = field_t<Tag, Position, Width>;
        auto current = static_cast<std::uint32_t>(value);
        current = (current & ~field::mask) | field::encode(new_value);
        value = static_cast<std::uint8_t>(current);
    }

    template<std::uint32_t... Masks>
    void write(field_value_t<Tag, Masks>... new_values) noexcept
    requires(sizeof...(Masks) > 0)
    {
        static_assert(masks_are_disjoint<Masks...>(), "bulk-written register fields must not overlap");
        constexpr auto combined_mask = (std::uint32_t {} | ... | Masks);
        const auto     combined_value = (std::uint32_t {} | ... | new_values.encoded_value);
        auto           current = static_cast<std::uint32_t>(value);
        current = (current & ~combined_mask) | combined_value;
        value = static_cast<std::uint8_t>(current);
    }
};

static_assert(sizeof(register_t<void>) == sizeof(std::uint32_t));
static_assert(sizeof(write_only_register_t<void>) == sizeof(std::uint32_t));
static_assert(sizeof(read_only_register_t<void>) == sizeof(std::uint32_t));
static_assert(sizeof(byte_register_t<void>) == sizeof(std::uint8_t));

