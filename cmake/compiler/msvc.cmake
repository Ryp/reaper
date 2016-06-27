if(MSVC)
    # Visual Studio 2015 or newer
    if(MSVC_VERSION LESS 1900)
        message(FATAL_ERROR "This version of Visual Studio is not supported")
    endif()

    set(REAPER_DEPS_LOCATION $ENV{REAPER_DEPS_LOCATION} CACHE PATH "Path to dependencies folder")
    if (REAPER_DEPS_LOCATION STREQUAL "")
        message(FATAL_ERROR "Please add the location of the dependencies folder")
    endif()

    # Set paths from the deps archive
    set(ASSIMP_ROOT_DIR ${REAPER_DEPS_LOCATION}/assimp-3.2 CACHE PATH "Assimp location")

    #add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
