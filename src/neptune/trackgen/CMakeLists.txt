#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target neptune_trackgen)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Track.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Track.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TrackDebug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TrackDebug.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TrackGenExport.h
)

target_link_libraries(${target} PUBLIC
    reaper_core
    reaper_math
    reaper_mesh
    glm
    span
)

set_target_properties(${target} PROPERTIES FOLDER Neptune)

reaper_configure_library(${target} "TrackGen")

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/gen_simple.cpp
)