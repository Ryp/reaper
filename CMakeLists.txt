cmake_minimum_required(VERSION 3.6 FATAL_ERROR)

# Avoid source tree pollution
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are discouraged. Make a separate folder for building:\nmkdir build; cd build; cmake ..;cd ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif()

# Remove RelWithDebInfo and MinSizeRel configuration types (call this before project())
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)

project(Reaper)

# Export compile commands in a json file. Useful for vim completion and debugging
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(REAPER_BUILD_KDEV_TEMPLATES      "Build custom templates for kdevelop" OFF)
option(REAPER_BUILD_TESTS               "Build tests" ON)
option(REAPER_BUILD_SHARED_LIBRARIES    "Build shared libraries" ON)

# Enable CTest
if (REAPER_BUILD_TESTS)
    enable_testing()
endif()

# Determine linkage type
if(REAPER_BUILD_SHARED_LIBRARIES)
    set(REAPER_BUILD_TYPE SHARED)
else()
    set(REAPER_BUILD_TYPE STATIC)
endif()

# Provide custom cmake scripts
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(cotire)
include(Reaper)

if(WIN32)
    set(VK_PLATFORM "VK_USE_PLATFORM_WIN32_KHR")
elseif(APPLE OR UNIX)
    set(VK_PLATFORM "VK_USE_PLATFORM_XCB_KHR")
endif()
add_definitions(-D${VK_PLATFORM})

set(REAPER_BIN reaper)

# Set precompiled header
set(REAPER_PCH ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h)

# Declare version numbers and pass them to Version.h
set(REAPER_VERSION_MAJOR 0)
set(REAPER_VERSION_MINOR 1)
set(REAPER_VERSION_PATCH 0)
set(REAPER_VERSION_SHORT "${REAPER_VERSION_MAJOR}.${REAPER_VERSION_MINOR}.${REAPER_VERSION_PATCH}")

configure_file(${CMAKE_SOURCE_DIR}/src/core/Version.h.in ${CMAKE_SOURCE_DIR}/src/core/Version.h NEWLINE_STYLE LF)

add_subdirectory(src/core)
add_subdirectory(src/gamelogic)
add_subdirectory(src/renderer)

set(REAPER_SRCS
    ${CMAKE_SOURCE_DIR}/src/main.cpp

    ${CMAKE_SOURCE_DIR}/src/cvar/CVar.cpp
    ${CMAKE_SOURCE_DIR}/src/cvar/CVar.h

#     ${CMAKE_SOURCE_DIR}/src/input/AbstractController.cpp
#     ${CMAKE_SOURCE_DIR}/src/input/AbstractController.h
#     ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.cpp
#     ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.h

    ${CMAKE_SOURCE_DIR}/src/math/BasicMath.h
    ${CMAKE_SOURCE_DIR}/src/math/Spline.cpp
    ${CMAKE_SOURCE_DIR}/src/math/Spline.h

    ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h

    ${CMAKE_SOURCE_DIR}/src/resource/Resource.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/Resource.h
)

add_executable(${REAPER_BIN} ${REAPER_SRCS})
reaper_add_custom_build_flags(${REAPER_BIN} "Reaper")

if (WIN32)
    reaper_vs_set_debugger_flags(${REAPER_BIN})
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${REAPER_BIN})

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_BIN} SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/external)

#Libraries
target_link_libraries(${REAPER_BIN}
    ${REAPER_CORE_BIN}
    ${REAPER_GAMELOGIC_BIN}
    ${REAPER_RENDERER_BIN})

set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_BIN})

if (REAPER_BUILD_KDEV_TEMPLATES)
    add_subdirectory(tools/kdevelop)
endif()
