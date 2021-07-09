#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_trackgen)
set(REAPER_TRACKGEN_BIN ${target} PARENT_SCOPE)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Track.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Track.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TrackDebug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TrackDebug.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TrackGenExport.h
)

target_link_libraries(${target} PUBLIC
    ${REAPER_CORE_BIN}
    ${REAPER_MATH_BIN}
    ${REAPER_MESH_BIN}
    glm
)

set_target_properties(${target} PROPERTIES FOLDER SplineSonic)

reaper_configure_library(${target} "TrackGen")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/gen_simple.cpp
)
