#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2020 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////
#
# Useful resources to keep in mind when reading/editing the build scripts:
# C++Now 2017: Daniel Pfeifer "Effective CMake"
# CppCon 2017: Mathieu Ropert "Using Modern CMake Patterns to Enforce a Good Modular Design"

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# Avoid source tree pollution
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        " In-source builds are discouraged. Tell CMake to use a separate folder:\n"
        " $ rm -rf CMakeCache.txt CMakeFiles\n"
        " $ cmake -H. -Bbuild")
endif()

get_property(REAPER_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(REAPER_MULTI_CONFIG)
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

# This will automatically keep git hooks up-to-date. Do NOT disable this option
# if you intent to push code from your local copy.
option(REAPER_GIT_HOOKS_AUTO_UPDATE     "Update git hooks"              ON)

# Enable testing. Remember to run the tests before submitting code.
option(REAPER_BUILD_TESTS               "Build tests"                   ON)

# Recommended option if you want to quickly iterate on libraries.
# The runtime performance should be comparable to a classic static build.
option(REAPER_BUILD_SHARED_LIBS         "Build shared libraries"        ${BUILD_SHARED_LIBS})

# Enable runtime profiling. Go to localhost:1338 in chrome to get the output.
option(REAPER_USE_MICROPROFILE          "Use microprofile"              ON)

# See .clang-tidy for the list of useful checks.
option(REAPER_RUN_CLANG_TIDY            "Run clang-tidy when compiling" ON)

# Building with coverage info will slow down build and execution times.
option(REAPER_BUILD_COVERAGE_INFO       "Build with coverage info"      OFF)

# Building with RenderDoc integration support.
# That allows us to capture frames in-engine and attach RenderDoc later to analyze them.
option(REAPER_USE_RENDERDOC             "Build with RenderDoc support"  OFF)

# This is experimental.
# See dxc.cmake for setup intructions
option(REAPER_HLSL_USE_DXC              "Build HLSL shaders with DXC"   OFF)

# Override binary output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})

# The CXX_STANDARD property is a requirement
set(CXX_STANDARD_REQUIRED ON)

# Export compile commands in a json file. Useful for clang-based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable verbose Makefile output. Useful for debugging
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Enable folder support (useful for Visual Studio)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Select linkage type
if(REAPER_BUILD_SHARED_LIBS)
    set(REAPER_BUILD_TYPE SHARED)
else()
    set(REAPER_BUILD_TYPE STATIC)
endif()

# Enable CTest
if (REAPER_BUILD_TESTS)
    enable_testing()
endif()

# Provide custom cmake scripts
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(Reaper)
include(cotire)

# Let CMake configure git hooks
reaper_setup_git_hooks()

add_subdirectory(src)
