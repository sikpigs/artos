if(CONFIG_ARTOS)
    target_sources(${FIRMWARE_TARGET} PRIVATE
        kernel.cpp
        thread.cpp
        scheduler.cpp
        blocking.cpp
        wait.cpp
        deferred.cpp
    )
endif()
