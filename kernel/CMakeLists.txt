if(CONFIG_ARTOS)
    target_include_directories(firmware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_sources(firmware PRIVATE
        kernel.cpp
        thread.cpp
        scheduler.cpp
        blocking.cpp
        wait.cpp
        deferred.cpp
        queue.cpp
    )
endif()
