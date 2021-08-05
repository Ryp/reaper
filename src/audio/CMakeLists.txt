#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2019 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_audio)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/alsa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/alsa.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WaveFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WaveFormat.h
)

target_link_libraries(${target} PUBLIC
    reaper_core
    asound
)

reaper_configure_library(${target} "Audio")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/alsa.cpp
)
