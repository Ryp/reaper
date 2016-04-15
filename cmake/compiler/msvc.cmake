if(MSVC)
    if(MSVC_VERSION LESS 1700)
        message(FATAL_ERROR "This version of Visual Studio is not supported")
    endif()

    set(REAPERGL_DEPS_LOCATION $ENV{REAPERGL_DEPS_LOCATION} CACHE PATH "Path to dependencies folder")
    if (REAPERGL_DEPS_LOCATION STREQUAL "")
        message(FATAL_ERROR "Please add the location of the dependencies folder")
    endif()

    #set(BOOST_ROOT ${REAPERGL_DEPS_LOCATION}/boost CACHE PATH "boost location")
    #add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
