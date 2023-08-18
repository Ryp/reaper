#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_mesh)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Mesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ModelLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ModelLoader.h
)

include(tinyobjloader)

target_link_libraries(${target} PUBLIC
    reaper_core
    glm
    tinyobjloader
)

reaper_configure_library(${target} "Mesh")

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/load.cpp
)
