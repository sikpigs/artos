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
    for (std::size_t index = 0; index < artos::thread::canary_word_count; ++index)
    {
        guard[index] = stack_fill_pattern;
    }
    const tcb_t thread { "test",
                         artos::thread::priority_t::low,
                         storage.usable_stack_begin(),
                         storage.usable_stack_begin(),
                         storage_t::usable_words,
                         guard };
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
