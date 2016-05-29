if(UNIX)
    find_package(Libunwind REQUIRED)

    set(REAPER_PLATFORM_LIBRARIES_UNIX
        ${LIBUNWIND_LIBRARIES}
        -lpthread -ldl
    )

    set(REAPER_PLATFORM_INCLUDES_UNIX
        ${LIBUNWIND_INCLUDE_DIR}
    )

    set(REAPER_PLATFORM_LIBRARIES ${REAPER_PLATFORM_LIBRARIES_UNIX} CACHE PATH "Additional platform-specific libraries")
    set(REAPER_PLATFORM_INCLUDES ${REAPER_PLATFORM_INCLUDES_UNIX} CACHE PATH "Additional platform-specific includes")
endif()
