#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2020 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/doctest)

#///////////////////////////////////////////////////////////////////////////////

add_library(fmt INTERFACE)
target_include_directories(fmt SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/fmt)
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

add_library(span INTERFACE)
target_include_directories(span SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/span-lite/include)

#///////////////////////////////////////////////////////////////////////////////

add_library(amd-vma INTERFACE)

set(VMA_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/amd-vma)

target_include_directories(amd-vma SYSTEM INTERFACE ${VMA_INCLUDE_DIR}/src)

if(MSVC)
    target_sources(amd-vma INTERFACE ${VMA_INCLUDE_DIR}/src/vk_mem_alloc.natvis)
endif()

#///////////////////////////////////////////////////////////////////////////////

add_library(tinyddsloader INTERFACE)

set(TINYDDSLOADER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/tinyddsloader)

target_include_directories(tinyddsloader SYSTEM INTERFACE ${TINYDDSLOADER_INCLUDE_DIR})

#///////////////////////////////////////////////////////////////////////////////
