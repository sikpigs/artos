#include "internal/thread_id.h"

#include <cstdint>

namespace {
    using layout32 = artos::thread::internal::thread_id_layout_t<std::uint32_t, 8>;
    using layout64 = artos::thread::internal::thread_id_layout_t<std::uint64_t, 8>;
    using layout10 = artos::thread::internal::thread_id_layout_t<std::uint32_t, 10>;

    static_assert(layout32::index_bits == 3);
    static_assert(layout32::index_mask == UINT32_C(0x00000007));
    static_assert(layout32::generation_mask == UINT32_C(0xfffffff8));
    static_assert(layout32::generation_increment == 8);

    static_assert(layout64::index_bits == 3);
    static_assert(layout64::index_mask == UINT64_C(0x0000000000000007));
    static_assert(layout64::generation_mask == UINT64_C(0xfffffffffffffff8));
    static_assert(layout64::generation_increment == 8);

    static_assert(layout10::index_bits == 4);
    static_assert(layout10::index_mask == UINT32_C(0x0000000f));

    static_assert(layout32::next(layout32::initial_id, 0) == 0);
    static_assert(layout32::next(0, 7) == 15);
    static_assert(layout64::next(layout64::initial_id, 0) == 0);
    static_assert(layout64::next(0, 7) == 15);

    static_assert(std::is_same_v<decltype(layout32::initial_id), const std::uint32_t>);
    static_assert(std::is_same_v<decltype(layout64::initial_id), const std::uint64_t>);
}

int main()
{
    return 0;
}
