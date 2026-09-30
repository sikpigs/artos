#include <internal/thread.h>

#include <artos.h>

#include <cstddef>
#include <cstdio>

int main()
{
    using artos::thread::internal::stack_fill_pattern;
    using artos::thread::internal::tcb_t;
    using storage_t = artos::thread::storage_t<artos::thread::minimum_stack_bytes>;

    storage_t   storage {};
    auto* const guard = storage.canary.data();
    auto* const stack_bytes = reinterpret_cast<std::byte*>(storage.stack.data());
    auto* const guard_end = reinterpret_cast<std::byte*>(guard + artos::thread::canary_word_count);
    if (guard_end != stack_bytes)
    {
        std::puts("canary is not adjacent to the stack");
        return 1;
    }

    for (std::size_t index = 0; index < artos::thread::canary_word_count; ++index)
    {
        guard[index] = stack_fill_pattern;
    }
    const tcb_t thread {
        "test", artos::thread::priority_t::low, storage.stack.data(), storage.stack.data(), storage.stack.size(), guard
    };
    if (!thread.canary_intact())
    {
        std::puts("intact canary was rejected");
        return 1;
    }
    guard[0] ^= 1U;
    if (thread.canary_intact())
    {
        std::puts("damaged canary was accepted");
        return 1;
    }
    return 0;
}
