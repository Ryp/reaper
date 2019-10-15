#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2018 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/doctest)

#///////////////////////////////////////////////////////////////////////////////

add_library(fmt INTERFACE)
target_include_directories(fmt SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/fmt)

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
