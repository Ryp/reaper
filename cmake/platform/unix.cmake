if(UNIX)
    find_package(moGL NO_MODULE REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(glbinding REQUIRED)
    find_package(OpenEXR REQUIRED)
    find_package(assimp REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(GLFW REQUIRED glfw3)

    find_package(Libunwind REQUIRED)

    set(REAPER_PLATFORM_LIBRARIES_UNIX "-lglbinding -lpthread -ldl"
        ${OPENGL_LIBRARIES}
        ${GLFW_LIBRARIES}
        ${GLBINDING_LIBRARY_RELEASE}
        ${ASSIMP_LIBRARIES}
        ${OPENEXR_LIBRARIES}
        ${LIBUNWIND_LIBRARIES}
    )

    set(REAPER_PLATFORM_INCLUDES_UNIX
        ${OPENGL_INCLUDE_DIRS}
        ${GLFW_INCLUDE_DIRS}
        ${MOGL_INCLUDE_DIRS}
        ${GLBINDING_INCLUDES}
        ${ASSIMP_INCLUDE_DIRS}
        ${OPENEXR_INCLUDE_DIRS}
        ${LIBUNWIND_INCLUDE_DIR}
    )

    set(REAPER_PLATFORM_LIBRARIES ${REAPER_PLATFORM_LIBRARIES_UNIX} CACHE PATH "Additional platform-specific libraries")
    set(REAPER_PLATFORM_INCLUDES ${REAPER_PLATFORM_INCLUDES_UNIX} CACHE PATH "Additional platform-specific includes")
endif()
