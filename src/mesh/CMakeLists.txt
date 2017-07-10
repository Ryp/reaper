#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

find_package(assimp REQUIRED)

if (WIN32)
    assimp_copy_binaries(${Reaper_BINARY_DIR})
endif()

set(REAPER_MESH_LIBRARIES
    ${REAPER_CORE_BIN}
    ${ASSIMP_LIBRARY}
)

set(REAPER_MESH_INCLUDES
    ${CMAKE_SOURCE_DIR}/external/glm
    ${ASSIMP_INCLUDE_DIRS}
)

set(REAPER_MESH_BIN reaper_mesh)
set(REAPER_MESH_BIN ${REAPER_MESH_BIN} PARENT_SCOPE)

set(REAPER_MESH_DIR ${CMAKE_SOURCE_DIR}/src/mesh)
set(REAPER_MESH_SRCS
    ${REAPER_MESH_DIR}/Mesh.cpp
    ${REAPER_MESH_DIR}/Mesh.h
    ${REAPER_MESH_DIR}/MeshCache.cpp
    ${REAPER_MESH_DIR}/MeshCache.h
    ${REAPER_MESH_DIR}/ModelLoader.cpp
    ${REAPER_MESH_DIR}/ModelLoader.h
)

set(REAPER_MESH_TEST_SRCS
    ${REAPER_MESH_DIR}/test/temp.cpp
)

add_library(${REAPER_MESH_BIN} ${REAPER_BUILD_TYPE} ${REAPER_MESH_SRCS})
reaper_add_custom_build_flags(${REAPER_MESH_BIN} "Mesh")

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_MESH_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_MESH_BIN} SYSTEM PUBLIC
    ${REAPER_MESH_INCLUDES})

#Libraries
target_link_libraries(${REAPER_MESH_BIN} ${REAPER_MESH_LIBRARIES})

set_target_properties(${REAPER_MESH_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_MESH_BIN})

if (REAPER_BUILD_TESTS)
    reaper_add_tests(${REAPER_MESH_BIN} ${REAPER_MESH_TEST_SRCS})
endif()
