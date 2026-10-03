if(CONFIG_ARTOS)
    target_include_directories(firmware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_sources(firmware PRIVATE
        kernel.cpp
        thread.cpp
        kernel_state.cpp
        scheduler.cpp
        blocking.cpp
        wait_queue.cpp
        deferred.cpp
        queue.cpp
        thread_queue.cpp
        context.cpp
        thread_util.cpp
        service_call.cpp
        mutex.cpp
    )
endif()
