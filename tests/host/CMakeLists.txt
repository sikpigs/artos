set(ARTOS_ROOT "${CMAKE_SOURCE_DIR}")

function(add_artos_native_test name source)
    add_executable(${name}_tests "${source}" ${ARGN})
    target_include_directories(${name}_tests PRIVATE
        "${ARTOS_ROOT}/include"
        "${ARTOS_ROOT}/port/include"
    )
    target_compile_definitions(${name}_tests PRIVATE ARTOS_HOST_TEST=1)
    target_compile_features(${name}_tests PRIVATE cxx_std_23)
    target_compile_options(${name}_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror -g -O0)
    add_test(NAME ${name} COMMAND ${name}_tests)
    set_tests_properties(${name} PROPERTIES LABELS native)
endfunction()

add_artos_native_test(intrusive_list "${ARTOS_ROOT}/tests/intrusive_list.cpp")
add_artos_native_test(expected "${ARTOS_ROOT}/tests/expected.cpp")
add_artos_native_test(thread_id "${ARTOS_ROOT}/tests/thread_id.cpp")
add_artos_native_test(stack_canary "${ARTOS_ROOT}/tests/stack_canary.cpp")
target_include_directories(stack_canary_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/kernel"
)
target_compile_definitions(stack_canary_tests PRIVATE ARTOS_TEST_THREAD_CANARY=1)
target_include_directories(thread_id_tests PRIVATE "${ARTOS_ROOT}/kernel")
add_artos_native_test(wait
    "${ARTOS_ROOT}/tests/wait.cpp"
    "${ARTOS_ROOT}/kernel/wait_queue.cpp"
)
add_artos_native_test(event
    "${ARTOS_ROOT}/tests/event.cpp"
    "${ARTOS_ROOT}/kernel/event.cpp"
    "${ARTOS_ROOT}/kernel/wait_queue.cpp"
)
target_include_directories(event_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/port/include"
    "${ARTOS_ROOT}/kernel"
)
add_artos_native_test(event_blocking
    "${ARTOS_ROOT}/tests/event_blocking.cpp"
    "${ARTOS_ROOT}/kernel/event.cpp"
    "${ARTOS_ROOT}/kernel/thread.cpp"
    "${ARTOS_ROOT}/kernel/blocking.cpp"
    "${ARTOS_ROOT}/kernel/wait_queue.cpp"
    "${ARTOS_ROOT}/kernel/thread_queue.cpp"
    "${ARTOS_ROOT}/kernel/kernel_state.cpp"
    "${ARTOS_ROOT}/kernel/scheduler.cpp"
    "${ARTOS_ROOT}/kernel/thread_util.cpp"
    "${ARTOS_ROOT}/kernel/mutex.cpp"
)
target_include_directories(event_blocking_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/port/include"
    "${ARTOS_ROOT}/kernel"
)
# Keep unrelated thread entry/port functions out of this host executable.
target_compile_options(event_blocking_tests PRIVATE -ffunction-sections -fdata-sections)
target_link_options(event_blocking_tests PRIVATE -Wl,--gc-sections)
set_tests_properties(event_blocking PROPERTIES TIMEOUT 30)
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

set(ARTOS_SCHEDULER_TEST_SOURCES
    "${ARTOS_ROOT}/tests/scheduler.cpp"
    "${ARTOS_ROOT}/kernel/scheduler.cpp"
    "${ARTOS_ROOT}/kernel/kernel_state.cpp"
    "${ARTOS_ROOT}/kernel/thread_util.cpp"
    "${ARTOS_ROOT}/kernel/wait_queue.cpp"
    "${ARTOS_ROOT}/kernel/thread_queue.cpp"
    "${ARTOS_ROOT}/kernel/context.cpp"
    "${ARTOS_ROOT}/kernel/mutex.cpp"
)
add_artos_native_test(scheduler ${ARTOS_SCHEDULER_TEST_SOURCES})
target_include_directories(scheduler_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/kernel"
)
add_artos_native_test(scheduler_aging ${ARTOS_SCHEDULER_TEST_SOURCES})
target_include_directories(scheduler_aging_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/kernel"
)
target_compile_definitions(scheduler_aging_tests PRIVATE ARTOS_TEST_PRIORITY_AGING=1)
add_artos_native_test(scheduler_stats ${ARTOS_SCHEDULER_TEST_SOURCES})
target_include_directories(scheduler_stats_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/kernel"
)
target_compile_definitions(scheduler_stats_tests PRIVATE ARTOS_TEST_THREAD_STATS=1)
# The scheduler needs mutex priority helpers, but not the blocking/CPU-port API.
foreach(scheduler_test IN ITEMS scheduler scheduler_aging scheduler_stats)
    target_compile_options(${scheduler_test}_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${scheduler_test}_tests PRIVATE -Wl,--gc-sections)
endforeach()
set_tests_properties(scheduler scheduler_aging scheduler_stats PROPERTIES TIMEOUT 30)

# Keep blocking and scheduling real; the host port simulates context switches.
set(ARTOS_MUTEX_TEST_SOURCES
    "${ARTOS_ROOT}/tests/mutex.cpp"
    "${ARTOS_ROOT}/kernel/mutex.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/support/event_stub.cpp"
    "${ARTOS_ROOT}/kernel/blocking.cpp"
    "${ARTOS_ROOT}/kernel/scheduler.cpp"
    "${ARTOS_ROOT}/kernel/kernel_state.cpp"
    "${ARTOS_ROOT}/kernel/thread_util.cpp"
    "${ARTOS_ROOT}/kernel/wait_queue.cpp"
    "${ARTOS_ROOT}/kernel/thread_queue.cpp"
)
foreach(mutex_test IN ITEMS mutex mutex_aging)
    add_artos_native_test(${mutex_test} ${ARTOS_MUTEX_TEST_SOURCES})
    target_include_directories(${mutex_test}_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
        "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
        "${ARTOS_ROOT}/kernel"
    )
    set_tests_properties(${mutex_test} PROPERTIES TIMEOUT 30)
endforeach()
target_compile_definitions(mutex_aging_tests PRIVATE ARTOS_TEST_PRIORITY_AGING=1)

foreach(semaphore_test IN ITEMS semaphore semaphore_aging)
    add_artos_native_test(${semaphore_test}
        "${ARTOS_ROOT}/tests/semaphore.cpp"
        "${ARTOS_ROOT}/kernel/semaphore.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/support/event_stub.cpp"
        "${ARTOS_ROOT}/kernel/mutex.cpp"
        "${ARTOS_ROOT}/kernel/blocking.cpp"
        "${ARTOS_ROOT}/kernel/scheduler.cpp"
        "${ARTOS_ROOT}/kernel/kernel_state.cpp"
        "${ARTOS_ROOT}/kernel/thread_util.cpp"
        "${ARTOS_ROOT}/kernel/wait_queue.cpp"
        "${ARTOS_ROOT}/kernel/thread_queue.cpp"
    )
    target_include_directories(${semaphore_test}_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
        "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
        "${ARTOS_ROOT}/kernel"
    )
    set_tests_properties(${semaphore_test} PROPERTIES TIMEOUT 30)
endforeach()
target_compile_definitions(semaphore_aging_tests PRIVATE ARTOS_TEST_PRIORITY_AGING=1)

foreach(queue_lifecycle_test IN ITEMS queue_lifecycle queue_lifecycle_aging)
    add_artos_native_test(${queue_lifecycle_test}
        "${ARTOS_ROOT}/tests/queue_lifecycle.cpp"
        "${ARTOS_ROOT}/kernel/queue.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/support/event_stub.cpp"
        "${ARTOS_ROOT}/kernel/mutex.cpp"
        "${ARTOS_ROOT}/kernel/blocking.cpp"
        "${ARTOS_ROOT}/kernel/scheduler.cpp"
        "${ARTOS_ROOT}/kernel/kernel_state.cpp"
        "${ARTOS_ROOT}/kernel/thread_util.cpp"
        "${ARTOS_ROOT}/kernel/wait_queue.cpp"
        "${ARTOS_ROOT}/kernel/thread_queue.cpp"
    )
    target_include_directories(${queue_lifecycle_test}_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
        "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
        "${ARTOS_ROOT}/kernel"
    )
    # Baseline cases run now; deletion cases remain explicitly pending until
    # queue_t exposes the upcoming API. Once exposed, all cases run normally.
    set_tests_properties(${queue_lifecycle_test} PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
endforeach()
target_compile_definitions(queue_lifecycle_aging_tests PRIVATE ARTOS_TEST_PRIORITY_AGING=1)

foreach(snapshot_test IN ITEMS thread_snapshot thread_snapshot_stats)
    add_artos_native_test(${snapshot_test}
        "${ARTOS_ROOT}/tests/thread_snapshot.cpp"
        "${ARTOS_ROOT}/kernel/thread.cpp"
        "${ARTOS_ROOT}/kernel/kernel_state.cpp"
    )
    target_include_directories(${snapshot_test}_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
        "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
        "${ARTOS_ROOT}/kernel"
    )
    target_compile_options(${snapshot_test}_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${snapshot_test}_tests PRIVATE -Wl,--gc-sections)
endforeach()
target_compile_definitions(thread_snapshot_stats_tests PRIVATE ARTOS_TEST_THREAD_STATS=1)

add_artos_native_test(thread_lifecycle
    "${ARTOS_ROOT}/tests/thread_lifecycle.cpp"
    "${ARTOS_ROOT}/kernel/thread.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/support/event_stub.cpp"
    "${ARTOS_ROOT}/kernel/blocking.cpp"
    "${ARTOS_ROOT}/kernel/scheduler.cpp"
    "${ARTOS_ROOT}/kernel/kernel_state.cpp"
    "${ARTOS_ROOT}/kernel/thread_util.cpp"
    "${ARTOS_ROOT}/kernel/wait_queue.cpp"
    "${ARTOS_ROOT}/kernel/thread_queue.cpp"
    "${ARTOS_ROOT}/kernel/mutex.cpp"
)
target_include_directories(thread_lifecycle_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
    "${ARTOS_ROOT}/kernel"
)
set_tests_properties(thread_lifecycle PROPERTIES TIMEOUT 30)

foreach(kill_test IN ITEMS thread_kill thread_kill_aging)
    add_artos_native_test(${kill_test}
        "${ARTOS_ROOT}/tests/thread_kill.cpp"
        "${ARTOS_ROOT}/kernel/thread.cpp"
        "${ARTOS_ROOT}/kernel/event.cpp"
        "${ARTOS_ROOT}/kernel/semaphore.cpp"
        "${ARTOS_ROOT}/kernel/queue.cpp"
        "${ARTOS_ROOT}/kernel/blocking.cpp"
        "${ARTOS_ROOT}/kernel/scheduler.cpp"
        "${ARTOS_ROOT}/kernel/kernel_state.cpp"
        "${ARTOS_ROOT}/kernel/thread_util.cpp"
        "${ARTOS_ROOT}/kernel/wait_queue.cpp"
        "${ARTOS_ROOT}/kernel/thread_queue.cpp"
        "${ARTOS_ROOT}/kernel/mutex.cpp"
    )
    target_include_directories(${kill_test}_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
        "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
        "${ARTOS_ROOT}/kernel"
    )
    set_tests_properties(${kill_test} PROPERTIES TIMEOUT 30)
endforeach()
target_compile_definitions(thread_kill_aging_tests PRIVATE ARTOS_TEST_PRIORITY_AGING=1)


foreach(suspend_test IN ITEMS thread_suspend thread_suspend_aging)
    add_artos_native_test(${suspend_test}
        "${ARTOS_ROOT}/tests/thread_suspend.cpp"
        "${ARTOS_ROOT}/kernel/thread.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/support/event_stub.cpp"
        "${ARTOS_ROOT}/kernel/blocking.cpp"
        "${ARTOS_ROOT}/kernel/scheduler.cpp"
        "${ARTOS_ROOT}/kernel/kernel_state.cpp"
        "${ARTOS_ROOT}/kernel/thread_util.cpp"
        "${ARTOS_ROOT}/kernel/wait_queue.cpp"
        "${ARTOS_ROOT}/kernel/thread_queue.cpp"
        "${ARTOS_ROOT}/kernel/mutex.cpp"
    )
    target_include_directories(${suspend_test}_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
        "${ARTOS_ROOT}/arch/arm/cortex_m4/include"
        "${ARTOS_ROOT}/kernel"
    )
    set_tests_properties(${suspend_test} PROPERTIES TIMEOUT 30)
endforeach()
target_compile_definitions(thread_suspend_aging_tests PRIVATE ARTOS_TEST_PRIORITY_AGING=1)

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
add_artos_python_test(semaphore_api)
add_artos_python_test(event_api)
add_artos_python_test(thread_kill_api)
add_artos_python_test(pre_commit_clang_format)
add_artos_python_test(pre_commit_clang_tidy)
add_artos_python_test(serial_console)
add_artos_python_test(svd_to_regmap)

set_tests_properties(
    python.pre_commit_clang_format
    python.pre_commit_clang_tidy
    PROPERTIES LABELS "python;hooks"
)
