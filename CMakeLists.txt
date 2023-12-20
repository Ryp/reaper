#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////
#
# Useful resources to keep in mind when reading/editing the build scripts:
# C++Now 2017: Daniel Pfeifer "Effective CMake"
# CppCon 2017: Mathieu Ropert "Using Modern CMake Patterns to Enforce a Good Modular Design"

cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

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
    VERSION 0.2.0
    LANGUAGES CXX
    DESCRIPTION "Tiny C++ Engine")

# This should be set to ON on CI builds at least.
option(REAPER_BUILD_WARNINGS_AS_ERRORS  "Treats warnings as errors"     OFF)

# This will automatically keep git hooks up-to-date. Do NOT disable this option
# if you intent to push code from your local copy.
option(REAPER_GIT_HOOKS_AUTO_UPDATE     "Update git hooks"              ON)

# Disables use of features that need a graphical display.
# This is especially useful for test machines.
option(REAPER_HEADLESS                  "Headless mode"                 OFF)

# Enable testing. Remember to run the tests before submitting code.
option(REAPER_BUILD_TESTS               "Build tests"                   ON)

# Recommended option if you want to quickly iterate on libraries.
# The runtime performance should be comparable to a classic static build.
option(REAPER_BUILD_SHARED_LIBS         "Build shared libraries"        ${BUILD_SHARED_LIBS})

# Enable runtime profiling with Tracy.
# Tracy has its own profiler app that you have to compile from source.
option(REAPER_USE_TRACY                 "Use Tracy"                     ON)

# Enable crash reporting with google breakpad
option(REAPER_USE_GOOGLE_BREAKPAD       "Use Google Breakpad"           OFF)

# Enable crash reporting with google crashpad
option(REAPER_USE_GOOGLE_CRASHPAD       "Use Google Crashpad"           ON)

# See .clang-tidy for the list of useful checks.
option(REAPER_RUN_CLANG_TIDY            "Run clang-tidy when compiling" OFF)

# Building with coverage info will slow down build and execution times.
option(REAPER_BUILD_COVERAGE_INFO       "Build with coverage info"      OFF)

# Building with RenderDoc integration support.
# That allows us to capture frames in-engine and attach RenderDoc later to analyze them.
option(REAPER_USE_RENDERDOC             "Build with RenderDoc support"  OFF)

# This is experimental.
# See dxc.cmake for setup intructions
option(REAPER_HLSL_USE_DXC              "Build HLSL shaders with DXC"   OFF)

# Enable physics simulation with bullet.
option(REAPER_USE_BULLET_PHYSICS        "Enable game physics"           ON)

if(REAPER_USE_GOOGLE_BREAKPAD AND REAPER_USE_GOOGLE_CRASHPAD)
    message(FATAL_ERROR "Google breakpad and crashpad can't be both enabled at the same time!")
endif()

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

# Write version string to text file
set(REAPER_VERSION_FILE ${CMAKE_BINARY_DIR}/version.txt)

# NOTE: newline is set to LF so that the deploy CI step code stays simple
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.txt.in ${REAPER_VERSION_FILE} NEWLINE_STYLE LF)
install(FILES ${REAPER_VERSION_FILE} DESTINATION ${CMAKE_INSTALL_PREFIX})

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

# Let CMake configure git hooks
reaper_setup_git_hooks()

add_subdirectory(src)
