#include <artos.h>
#include <board_devices.h>
#include <type_traits>

// Compile-only checks: descriptions retain node identity, and invalid public
// API inputs are rejected before any firmware can run.
static_assert(std::is_same_v<board::chosen::console, board::labels::console_uart>);

template<std::uint32_t Bytes>
concept valid_storage = requires { typename artos::thread::thread_storage_t<Bytes>; };
static_assert(valid_storage<artos::thread::minimum_stack_bytes>);
static_assert(valid_storage<4096>);
static_assert(!valid_storage<72>);
static_assert(!valid_storage<256>);
static_assert(!valid_storage<artos::thread::minimum_stack_bytes - artos::port::stack_alignment>);
static_assert(!valid_storage<artos::thread::minimum_stack_bytes + 1>);
static_assert(!valid_storage<0>);
static_assert(!valid_storage<56>);
static_assert(!valid_storage<65>);

template<std::uint8_t Priority>
concept valid_priority = requires { artos::thread::make_priority<Priority>(); };
static_assert(valid_priority<0>);
static_assert(valid_priority<build_config::artos::priority_levels - 1>);
// The uint8_t priority type can represent every priority when there are 256.
static_assert(build_config::artos::priority_levels == 256
              || !valid_priority<static_cast<std::uint8_t>(build_config::artos::priority_levels)>);

// Instantiate the body, not just its declaration: catches forwarding mistakes
// that the monitor-only firmware build otherwise does not exercise.
artos::thread::start_result_t check_thread_create(
    artos::thread::thread_storage_t<artos::thread::minimum_stack_bytes>& storage)
{
    return artos::thread::create(+[](void*) {}, "check", nullptr, artos::thread::make_priority<0>(), storage);
}
