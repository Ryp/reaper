#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_mesh)
set(REAPER_MESH_BIN ${target} PARENT_SCOPE)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Mesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MeshCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ModelLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ModelLoader.h
)

include(tinyobjloader)

target_link_libraries(${target} PUBLIC
    ${REAPER_CORE_BIN}
    glm
    tinyobjloader
)

reaper_configure_library(${target} "Mesh")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/load.cpp
)
