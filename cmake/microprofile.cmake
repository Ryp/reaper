#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(MICROPROFILE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/microprofile)

configure_file(${CMAKE_SOURCE_DIR}/external/microprofile.config.h.in ${MICROPROFILE_INCLUDE_DIR}/microprofile.config.h NEWLINE_STYLE LF)

add_definitions(-DMICROPROFILE_USE_CONFIG)

set(MICROPROFILE_LIBRARY microprofile)
set(MICROPROFILE_SRCS
    ${CMAKE_SOURCE_DIR}/external/microprofile.config.h.in
    ${CMAKE_SOURCE_DIR}/external/microprofile/microprofile.config.h
    ${CMAKE_SOURCE_DIR}/external/microprofile/microprofile.cpp
    ${CMAKE_SOURCE_DIR}/external/microprofile/microprofile.h
)

add_library(${MICROPROFILE_LIBRARY} STATIC ${MICROPROFILE_SRCS})

if (MSVC)
    set_target_properties(${MICROPROFILE_LIBRARY} PROPERTIES FOLDER External)
endif()

if (WIN32)
    target_link_libraries(${MICROPROFILE_LIBRARY} ws2_32)
endif()

