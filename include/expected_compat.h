#pragma once

// A header may exist even when this compiler cannot use its implementation.
#if __has_include(<expected>)
#include <expected>
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
namespace artos {
    using std::expected;
    using std::unexpected;
}
#else
#include <expected.h>
#endif
