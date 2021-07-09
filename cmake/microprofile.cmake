#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(MICROPROFILE_BIN microprofile)

add_library(${MICROPROFILE_BIN} ${REAPER_BUILD_TYPE})

target_sources(${MICROPROFILE_BIN} PRIVATE
    ${CMAKE_SOURCE_DIR}/external/microprofile.config.h.in
    ${CMAKE_SOURCE_DIR}/external/microprofile/microprofile.config.h
    ${CMAKE_SOURCE_DIR}/external/microprofile/microprofile.cpp
    ${CMAKE_SOURCE_DIR}/external/microprofile/microprofile.h
)

set(MICROPROFILE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/microprofile)

configure_file(${CMAKE_SOURCE_DIR}/external/microprofile.config.h.in ${MICROPROFILE_INCLUDE_DIR}/microprofile.config.h NEWLINE_STYLE UNIX)

target_include_directories(${MICROPROFILE_BIN} SYSTEM INTERFACE ${MICROPROFILE_INCLUDE_DIR})

# Get microprofile to include our custom conf
target_compile_definitions(${MICROPROFILE_BIN} PUBLIC MICROPROFILE_USE_CONFIG)

reaper_configure_external_target(${MICROPROFILE_BIN} "Microprofile")

set_target_properties(${MICROPROFILE_BIN} PROPERTIES FOLDER External)

find_package(Vulkan REQUIRED)

target_link_libraries(${MICROPROFILE_BIN} PRIVATE Vulkan::Vulkan)

# NOTE: microprofile links its own libvulkan, because there's no easy way to provide our own symbols
if(UNIX)
    target_link_libraries(${MICROPROFILE_BIN} PRIVATE pthread)
elseif(WIN32)
    target_link_libraries(${MICROPROFILE_BIN} PRIVATE ws2_32)
endif()
