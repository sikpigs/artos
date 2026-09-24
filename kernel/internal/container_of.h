#pragma once

#include <cstddef>
#include <type_traits>

namespace artos {
    template<typename Container, typename Member>
    Container* container_of(Member* pointer, std::size_t offset) noexcept
    {
        static_assert(std::is_standard_layout_v<Container>);

        auto* bytes = reinterpret_cast<std::byte*>(pointer);
        return reinterpret_cast<Container*>(bytes - offset);
    }

    template<typename Container, typename Member>
    const Container* container_of(const Member* pointer, std::size_t offset) noexcept
    {
        static_assert(std::is_standard_layout_v<Container>);

        const auto* bytes = reinterpret_cast<const std::byte*>(pointer);
        return reinterpret_cast<const Container*>(bytes - offset);
    }

}

#define ARTOS_CONTAINER_OF(pointer, type, member) ::artos::container_of<type>((pointer), offsetof(type, member))
