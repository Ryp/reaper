if(UNIX)
    set(REAPER_PLATFORM_LIBRARIES_UNIX "-lglbinding -lpthread -ldl")
    set(REAPER_PLATFORM_INCLUDES_UNIX "")

    set(REAPER_PLATFORM_LIBRARIES ${REAPER_PLATFORM_LIBRARIES_UNIX} CACHE PATH "Additional platform-specific libraries")
    set(REAPER_PLATFORM_INCLUDES ${REAPER_PLATFORM_INCLUDES_UNIX} CACHE PATH "Additional platform-specific includes")
endif()
