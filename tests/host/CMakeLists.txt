cmake_minimum_required(VERSION 3.24)
project(artos_host_tests LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()
add_executable(intrusive_list_tests ../intrusive_list.cpp)
target_include_directories(intrusive_list_tests PRIVATE ../../include)
target_compile_features(intrusive_list_tests PRIVATE cxx_std_23)
target_compile_options(intrusive_list_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -g -O0)
add_test(NAME intrusive_list COMMAND intrusive_list_tests)

add_executable(expected_tests ../expected.cpp)
target_include_directories(expected_tests PRIVATE ../../include)
target_compile_features(expected_tests PRIVATE cxx_std_23)
target_compile_options(expected_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -g -O0)
add_test(NAME expected COMMAND expected_tests)

# Exercise the same automatic selection used by firmware consumers.
add_executable(expected_auto_tests ../expected.cpp)
target_include_directories(expected_auto_tests PRIVATE ../../include)
target_compile_features(expected_auto_tests PRIVATE cxx_std_23)
target_compile_definitions(expected_auto_tests PRIVATE ARTOS_TEST_AUTO_EXPECTED=1)
target_compile_options(expected_auto_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -g -O0)
add_test(NAME expected_auto COMMAND expected_auto_tests)

# Run the same contract against the standard implementation when available.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c++23")
check_cxx_source_compiles("
    #include <expected>
    #if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
    #error std::expected unavailable
    #endif
    int main() { std::expected<int, int> value{1}; return *value - 1; }
" ARTOS_HOST_HAS_STD_EXPECTED)
unset(CMAKE_REQUIRED_FLAGS)
if(ARTOS_HOST_HAS_STD_EXPECTED)
    add_executable(expected_reference_tests ../expected.cpp)
    target_compile_features(expected_reference_tests PRIVATE cxx_std_23)
    target_compile_definitions(expected_reference_tests PRIVATE ARTOS_TEST_STD_EXPECTED=1)
    target_compile_options(expected_reference_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -g -O0)
    add_test(NAME expected_reference COMMAND expected_reference_tests)
endif()
