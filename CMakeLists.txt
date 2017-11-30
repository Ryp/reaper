#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

# Avoid source tree pollution
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are discouraged. Make a separate folder for building:\nmkdir build; cd build; cmake ..;cd ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif()

if(GENERATOR_IS_MULTI_CONFIG)
    # Remove RelWithDebInfo and MinSizeRel configuration types (call this before project())
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
else()
    # Monoconfig generator
    set(REAPER_DEFAULT_BUILD_TYPE "Debug")
    if(NOT CMAKE_BUILD_TYPE)
        message(STATUS "No build type provided, defaulting to: ${REAPER_DEFAULT_BUILD_TYPE}")
        set(CMAKE_BUILD_TYPE ${REAPER_DEFAULT_BUILD_TYPE} CACHE STRING "" FORCE)
    endif()
    # Set the valid options for cmake-gui drop-down list
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug;Release")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY HELPSTRING "Choose the type of build")
endif()

project(Reaper
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Tiny C++ Engine")

# Export compile commands in a json file. Useful for clang-based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(REAPER_GIT_HOOKS_AUTO_UPDATE     "Update git hooks"              ON)
option(REAPER_BUILD_TESTS               "Build tests"                   ON)
option(REAPER_BUILD_SHARED_LIBRARIES    "Build shared libraries"        ON)
option(REAPER_USE_MICROPROFILE          "Use microprofile"              ON)
option(REAPER_RUN_CLANG_TIDY            "Run clang-tidy when compiling" ON)

# Enable CTest
if (REAPER_BUILD_TESTS)
    enable_testing()
endif()

# Select linkage type
if(REAPER_BUILD_SHARED_LIBRARIES)
    set(REAPER_BUILD_TYPE SHARED)
else()
    set(REAPER_BUILD_TYPE STATIC)
endif()

# Provide custom cmake scripts
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(Reaper)

# Auto-update git hooks
if(REAPER_GIT_HOOKS_AUTO_UPDATE)
    include(clang-format)

    # Check if the executable is available
    if(NOT CLANG_FORMAT)
        message(FATAL_ERROR "clang-format is required")
    endif()

    set(REAPER_CLANG_FORMAT_REQUIRED_VERSION 6.0.0)

    # Check that we meet the requirements
    if(CLANG_FORMAT_VERSION VERSION_LESS ${REAPER_CLANG_FORMAT_REQUIRED_VERSION})
        message(FATAL_ERROR "clang-format ${REAPER_CLANG_FORMAT_REQUIRED_VERSION} is required")
    endif()

    reaper_update_git_hooks()
endif()

include(cotire)
include(glslang)

if(REAPER_USE_MICROPROFILE)
    add_definitions(-DREAPER_USE_MICROPROFILE) # Also define the macro for C++ code
    add_definitions(-DMICROPROFILE_USE_CONFIG) # Get microprofile to include our custom conf
    include(microprofile)
endif()

set(REAPER_BIN reaper)

# Set precompiled header
set(REAPER_PCH ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h)

configure_file(${CMAKE_SOURCE_DIR}/src/core/Version.h.in ${CMAKE_SOURCE_DIR}/src/core/Version.h NEWLINE_STYLE UNIX)

# Choose windowing system
if(WIN32)
    set(REAPER_USE_WIN32_WINDOW ON)
    set(VK_PLATFORM "VK_USE_PLATFORM_WIN32_KHR")
elseif(APPLE OR UNIX)
    set(REAPER_USE_XCB_WINDOW ON)
    set(VK_PLATFORM "VK_USE_PLATFORM_XCB_KHR")
    #set(REAPER_USE_XLIB_WINDOW ON)
    #set(VK_PLATFORM "VK_USE_PLATFORM_XLIB_KHR")
else()
    # We could allow headless instances without problems
    message(FATAL_ERROR "Could not detect platform!")
endif()
add_definitions(-D${VK_PLATFORM})

add_subdirectory(src/core)
add_subdirectory(src/math)
add_subdirectory(src/mesh)
add_subdirectory(src/common)
add_subdirectory(src/gamelogic)
add_subdirectory(src/renderer)

add_subdirectory(src/splinesonic)

set(REAPER_SRCS
    ${CMAKE_SOURCE_DIR}/src/main.cpp

    ${CMAKE_SOURCE_DIR}/src/AssetCache.cpp
    ${CMAKE_SOURCE_DIR}/src/AssetCache.h

    ${CMAKE_SOURCE_DIR}/src/cvar/CVar.cpp
    ${CMAKE_SOURCE_DIR}/src/cvar/CVar.h

#     ${CMAKE_SOURCE_DIR}/src/input/AbstractController.cpp
#     ${CMAKE_SOURCE_DIR}/src/input/AbstractController.h
#     ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.cpp
#     ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.h

    ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h

    ${CMAKE_SOURCE_DIR}/src/resource/Resource.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/Resource.h
)

# List additionnal natvis files for Visual Studio
if(MSVC)
    set(REAPER_NATVIS_FILES
        ${CMAKE_SOURCE_DIR}/external/glm/util/glm.natvis
    )
endif()

add_executable(${REAPER_BIN} ${REAPER_SRCS} ${REAPER_NATVIS_FILES})
reaper_configure_executable(${REAPER_BIN} "Reaper")

# Configure main VS projet
set_target_properties(${REAPER_BIN} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${REAPER_BIN})

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_BIN} SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/external)

#Libraries
target_link_libraries(${REAPER_BIN}
    ${MICROPROFILE_BIN}
    ${REAPER_CORE_BIN}
    ${REAPER_COMMON_BIN}
    ${REAPER_GAMELOGIC_BIN}
    ${REAPER_MATH_BIN}
    ${REAPER_MESH_BIN}
    ${REAPER_RENDERER_BIN})

set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_BIN})
