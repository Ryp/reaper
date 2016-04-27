if(UNIX)
    set(PLATFORM_LIBRARIES_UNIX -lglbinding -lpthread -ldl)
    set(PLATFORM_INCLUDES_UNIX "")

    set(PLATFORM_LIBRARIES PLATFORM_LIBRARIES_UNIX CACHE PATH "Additional platform-specific libraries")
    set(PLATFORM_INCLUDES PLATFORM_INCLUDES_UNIX CACHE PATH "Additional platform-specific includes")
endif()
