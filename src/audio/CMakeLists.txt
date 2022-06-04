#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_audio)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WaveFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WaveFormat.h
)

target_link_libraries(${target} PUBLIC
    reaper_core
    reaper_common
    fmt
)

if(UNIX)
    find_package(ALSA REQUIRED)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/alsa.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/alsa.h
    )
    target_link_libraries(${target} PRIVATE ${ALSA_LIBRARIES})
    target_include_directories(${target} PUBLIC ${ALSA_INCLUDE_DIRS}) # FIXME make this private
    target_compile_definitions(${target} PUBLIC REAPER_USE_ALSA)
endif()

reaper_configure_library(${target} "Audio")

if(UNIX)
    reaper_add_tests(${target}
        ${CMAKE_CURRENT_SOURCE_DIR}/test/alsa.cpp
    )
endif()
