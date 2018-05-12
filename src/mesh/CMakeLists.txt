#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2018 Thibault Schueller
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

# Add assimp dependency
find_package(assimp REQUIRED)
target_link_libraries(${target} PUBLIC ${ASSIMP_LIBRARY})
target_include_directories(${target} SYSTEM PUBLIC ${ASSIMP_INCLUDE_DIRS})

if(WIN32)
    assimp_copy_binaries(${Reaper_BINARY_DIR})
endif()

target_link_libraries(${target} PUBLIC
    ${REAPER_CORE_BIN}
    glm
)

reaper_configure_library(${target} "Mesh")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/load.cpp
)
