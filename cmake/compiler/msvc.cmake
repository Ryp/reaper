if(MSVC)
    # Visual Studio 2015 or newer
    if(MSVC_VERSION LESS 1900)
        message(FATAL_ERROR "This version of Visual Studio is not supported")
    endif()

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_USE_MATH_DEFINES)
endif()
