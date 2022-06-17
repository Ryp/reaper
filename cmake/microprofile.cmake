#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(MICROPROFILE_BIN microprofile)

add_library(${MICROPROFILE_BIN} ${REAPER_BUILD_TYPE})

set(MICROPROFILE_PATH ${CMAKE_SOURCE_DIR}/external/microprofile)

target_sources(${MICROPROFILE_BIN} PRIVATE
    ${CMAKE_SOURCE_DIR}/external/microprofile.config.h.in
    ${MICROPROFILE_PATH}/microprofile.config.h
    ${MICROPROFILE_PATH}/microprofile.cpp
    ${MICROPROFILE_PATH}/microprofile.h
)

set(MICROPROFILE_INCLUDE_DIR ${MICROPROFILE_PATH})

configure_file(${CMAKE_SOURCE_DIR}/external/microprofile.config.h.in ${MICROPROFILE_INCLUDE_DIR}/microprofile.config.h)

target_include_directories(${MICROPROFILE_BIN} SYSTEM INTERFACE ${MICROPROFILE_INCLUDE_DIR})

# Get microprofile to include our custom conf
target_compile_definitions(${MICROPROFILE_BIN} PUBLIC MICROPROFILE_USE_CONFIG)

reaper_configure_external_target(${MICROPROFILE_BIN} "Microprofile" ${MICROPROFILE_PATH})

set_target_properties(${MICROPROFILE_BIN} PROPERTIES FOLDER External)

target_link_libraries(${MICROPROFILE_BIN} PUBLIC reaper_vulkan_loader)

if(UNIX)
    target_link_libraries(${MICROPROFILE_BIN} PRIVATE pthread)
elseif(WIN32)
    target_link_libraries(${MICROPROFILE_BIN} PRIVATE ws2_32)
endif()
