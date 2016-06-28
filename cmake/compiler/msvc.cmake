if(MSVC)
    # Visual Studio 2015 or newer
    if(MSVC_VERSION LESS 1900)
        message(FATAL_ERROR "This version of Visual Studio is not supported")
    endif()

    set(REAPER_DEPS_LOCATION ${CMAKE_SOURCE_DIR}/external/downloaded)
    set(REAPER_DEPS_ARCHIVE ${REAPER_DEPS_LOCATION}/deps.tar.gz)

    file(DOWNLOAD "https://www.dropbox.com/s/1a0ahbhsjsljmdw/reaper-deps-r1-2016-06-28.tar.gz?dl=0" ${REAPER_DEPS_ARCHIVE}
        EXPECTED_HASH MD5=857a1318f9fde1efb355c427d9bf94ff
        INACTIVITY_TIMEOUT 10
        SHOW_PROGRESS)

    message(STATUS "Extracting deps archive")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${REAPER_DEPS_ARCHIVE}
        WORKING_DIRECTORY ${REAPER_DEPS_LOCATION})

    # Set paths from the deps archive
    set(ASSIMP_ROOT_DIR ${REAPER_DEPS_LOCATION}/assimp-3.2 CACHE PATH "Assimp location")

    #add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
