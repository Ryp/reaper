#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

add_library(lodepng STATIC)

set(LODEPNG_PATH ${CMAKE_SOURCE_DIR}/external/lodepng)

target_sources(lodepng PRIVATE
    ${LODEPNG_PATH}/lodepng.cpp
    ${LODEPNG_PATH}/lodepng.h
)

set_target_properties(lodepng PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(lodepng SYSTEM INTERFACE ${LODEPNG_PATH})

#///////////////////////////////////////////////////////////////////////////////

add_library(cgltf STATIC)

target_sources(cgltf PRIVATE
    ${CMAKE_SOURCE_DIR}/external/cgltf_impl.cpp
)

set_target_properties(cgltf PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(cgltf SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/external/cgltf)

#///////////////////////////////////////////////////////////////////////////////

add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/doctest)

#///////////////////////////////////////////////////////////////////////////////

add_library(fmt INTERFACE)
target_include_directories(fmt SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/fmt/include)
target_compile_definitions(fmt INTERFACE FMT_HEADER_ONLY)

#///////////////////////////////////////////////////////////////////////////////

add_library(gli INTERFACE)
target_include_directories(gli SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/gli)

#///////////////////////////////////////////////////////////////////////////////

set(GLM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/glm)

add_library(glm INTERFACE)
target_include_directories(glm SYSTEM INTERFACE ${GLM_INCLUDE_DIR})
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE)

if(MSVC)
    target_sources(glm INTERFACE ${GLM_INCLUDE_DIR}/util/glm.natvis)
endif()

#///////////////////////////////////////////////////////////////////////////////

add_library(amd-vma INTERFACE)

set(VMA_PATH ${CMAKE_SOURCE_DIR}/external/amd-vma)

target_include_directories(amd-vma SYSTEM INTERFACE ${VMA_PATH}/include)

if(MSVC)
    target_sources(amd-vma INTERFACE ${VMA_PATH}/src/vk_mem_alloc.natvis)
endif()

#///////////////////////////////////////////////////////////////////////////////

add_library(tinyddsloader INTERFACE)

set(TINYDDSLOADER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/tinyddsloader)

target_include_directories(tinyddsloader SYSTEM INTERFACE ${TINYDDSLOADER_INCLUDE_DIR})

#///////////////////////////////////////////////////////////////////////////////

add_library(inih STATIC)

set(INIH_PATH ${CMAKE_SOURCE_DIR}/external/inih)

target_sources(inih PRIVATE
    ${INIH_PATH}/ini.c
)

target_include_directories(inih SYSTEM INTERFACE ${INIH_PATH})

#///////////////////////////////////////////////////////////////////////////////
