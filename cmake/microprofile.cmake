#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(MICROPROFILE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/microprofile)

set(MICROPROFILE_BIN microprofile)

set(MICROPROFILE_SRCS
    ${CMAKE_SOURCE_DIR}/external/microprofile.config.h.in
    ${CMAKE_SOURCE_DIR}/external/microprofile/microprofile.config.h
    ${CMAKE_SOURCE_DIR}/external/microprofile/microprofile.cpp
    ${CMAKE_SOURCE_DIR}/external/microprofile/microprofile.h
)

configure_file(${CMAKE_SOURCE_DIR}/external/microprofile.config.h.in ${MICROPROFILE_INCLUDE_DIR}/microprofile.config.h NEWLINE_STYLE UNIX)

add_library(${MICROPROFILE_BIN} ${REAPER_BUILD_TYPE} ${MICROPROFILE_SRCS})
reaper_configure_external_target(${MICROPROFILE_BIN} "Microprofile")

# Get microprofile to include our custom conf
target_compile_definitions(${MICROPROFILE_BIN} PUBLIC MICROPROFILE_USE_CONFIG)

set_target_properties(${MICROPROFILE_BIN} PROPERTIES FOLDER External)

if(WIN32)
    target_link_libraries(${MICROPROFILE_BIN} ws2_32)
endif()

