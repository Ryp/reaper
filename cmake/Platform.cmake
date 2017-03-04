#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

if(UNIX)
    # Nothing yet
elseif(WIN32)
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
else()
    message(FATAL_ERROR "Unrecognized platform")
endif()
