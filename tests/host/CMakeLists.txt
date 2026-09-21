set(ARTOS_ROOT "${CMAKE_SOURCE_DIR}")

function(add_artos_native_test name source)
    add_executable(${name}_tests "${source}" ${ARGN})
    target_include_directories(${name}_tests PRIVATE "${ARTOS_ROOT}/include")
    target_compile_features(${name}_tests PRIVATE cxx_std_23)
    target_compile_options(${name}_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror -g -O0)
    add_test(NAME ${name} COMMAND ${name}_tests)
    set_tests_properties(${name} PROPERTIES LABELS native)
endfunction()

add_artos_native_test(intrusive_list "${ARTOS_ROOT}/tests/intrusive_list.cpp")
add_artos_native_test(expected "${ARTOS_ROOT}/tests/expected.cpp")
add_artos_native_test(thread_id "${ARTOS_ROOT}/tests/thread_id.cpp")
target_include_directories(thread_id_tests PRIVATE "${ARTOS_ROOT}/kernel")
add_artos_native_test(wait
    "${ARTOS_ROOT}/tests/wait.cpp"
    "${ARTOS_ROOT}/kernel/wait.cpp"
)
add_artos_native_test(queue
    "${ARTOS_ROOT}/tests/queue.cpp"
    "${ARTOS_ROOT}/kernel/queue.cpp"
)
target_include_directories(queue_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/kernel"
)

add_artos_native_test(cxa_guard
    "${ARTOS_ROOT}/tests/cxa_guard.cpp"
    "${ARTOS_ROOT}/runtime/cxa_guard.cpp"
)
target_include_directories(cxa_guard_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/kernel"
)
target_compile_definitions(cxa_guard_tests PRIVATE
    __cxa_guard_acquire=artos_test_guard_acquire
    __cxa_guard_release=artos_test_guard_release
    __cxa_guard_abort=artos_test_guard_abort
)

add_artos_native_test(scheduler
    "${ARTOS_ROOT}/tests/scheduler.cpp"
    "${ARTOS_ROOT}/kernel/scheduler.cpp"
    "${ARTOS_ROOT}/kernel/wait.cpp"
)
target_include_directories(scheduler_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/kernel"
)
target_compile_definitions(scheduler_tests PRIVATE ARTOS_HOST_TEST=1)
set_tests_properties(scheduler PROPERTIES TIMEOUT 30)

# Exercise the same automatic selection used by firmware consumers.
add_artos_native_test(expected_auto "${ARTOS_ROOT}/tests/expected.cpp")
target_compile_definitions(expected_auto_tests PRIVATE ARTOS_TEST_AUTO_EXPECTED=1)

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
    add_artos_native_test(expected_reference "${ARTOS_ROOT}/tests/expected.cpp")
    target_compile_definitions(expected_reference_tests PRIVATE ARTOS_TEST_STD_EXPECTED=1)
endif()

# Prefer the project-local environment, matching the firmware configuration.
if(NOT DEFINED Python3_EXECUTABLE AND EXISTS "${ARTOS_ROOT}/.venv/bin/python")
    set(Python3_EXECUTABLE "${ARTOS_ROOT}/.venv/bin/python")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

function(add_artos_python_test name)
    add_test(NAME python.${name}
        COMMAND "${Python3_EXECUTABLE}" "${ARTOS_ROOT}/tests/test_${name}.py")
    set_tests_properties(python.${name} PROPERTIES
        LABELS python
        TIMEOUT 180
        WORKING_DIRECTORY "${ARTOS_ROOT}"
    )
endfunction()

add_artos_python_test(artos_cli)
add_artos_python_test(dtb_to_board)
add_artos_python_test(kconfig)
add_artos_python_test(pre_commit_clang_format)
add_artos_python_test(pre_commit_clang_tidy)
add_artos_python_test(serial_console)
add_artos_python_test(svd_to_regmap)

set_tests_properties(
    python.pre_commit_clang_format
    python.pre_commit_clang_tidy
    PROPERTIES LABELS "python;hooks"
)
