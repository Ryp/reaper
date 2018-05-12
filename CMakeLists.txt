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

# This will automatically keep git hooks up-to-date. Do NOT disable this option
# if you intent to push code on your local copy.
option(REAPER_GIT_HOOKS_AUTO_UPDATE     "Update git hooks"              ON)

# Enable testing. Remember to run the tests before submitting code.
option(REAPER_BUILD_TESTS               "Build tests"                   ON)

# Recommended option if you want to quickly iterate on libraries.
# The runtime performance should be comparable to a classic static build.
option(REAPER_BUILD_SHARED_LIBRARIES    "Build shared libraries"        ON)

# Enable runtime profiling. Go to localhost:1338 in chrome to get the output.
option(REAPER_USE_MICROPROFILE          "Use microprofile"              ON)

# See .clang-tidy for the list of useful checks.
option(REAPER_RUN_CLANG_TIDY            "Run clang-tidy when compiling" ON)

# Building with coverage info will slow down build and execution times.
option(REAPER_BUILD_COVERAGE_INFO       "Build with coverage info"      OFF)

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
include(cotire)

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

add_subdirectory(src)
