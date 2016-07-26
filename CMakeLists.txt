cmake_minimum_required(VERSION 3.6 FATAL_ERROR)

# Avoid source tree pollution
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are discouraged. Make a separate folder for building:\nmkdir build; cd build; cmake ..;cd ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif()

# Remove RelWithDebInfo and MinSizeRel configuration types (call this before project())
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)

project(Reaper)

option(REAPER_BUILD_KDEV_TEMPLATES      "Build custom templates for kdevelop" OFF)
option(REAPER_BUILD_TESTS               "Build tests" ON)
option(REAPER_BUILD_SHARED_LIBRARIES    "Build shared libraries" ON)

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

set(REAPER_BIN reaper)

# Set precompiled header
set(REAPER_PCH ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h)

add_subdirectory(src/core)
add_subdirectory(src/renderer)

set(REAPER_SRCS
    ${CMAKE_SOURCE_DIR}/src/main.cpp

    ${CMAKE_SOURCE_DIR}/src/cvar/CVar.cpp
    ${CMAKE_SOURCE_DIR}/src/cvar/CVar.h

    ${CMAKE_SOURCE_DIR}/src/game/EntityDescriptor.h
    ${CMAKE_SOURCE_DIR}/src/game/ModuleUpdater.cpp
    ${CMAKE_SOURCE_DIR}/src/game/ModuleUpdater.h
    ${CMAKE_SOURCE_DIR}/src/game/WorldUpdater.cpp
    ${CMAKE_SOURCE_DIR}/src/game/WorldUpdater.h

    ${CMAKE_SOURCE_DIR}/src/game/entitydb/EntityDb.cpp
    ${CMAKE_SOURCE_DIR}/src/game/entitydb/EntityDb.h

    ${CMAKE_SOURCE_DIR}/src/game/map/MapDescriptor.h
    ${CMAKE_SOURCE_DIR}/src/game/map/MapInfo.cpp
    ${CMAKE_SOURCE_DIR}/src/game/map/MapInfo.h

    ${CMAKE_SOURCE_DIR}/src/game/module/DamageModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/DamageModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/MovementModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/MovementModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/PathModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/PathModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/PositionModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/PositionModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/TeamModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/TeamModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/TowerControllerModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/TowerControllerModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/WaveSpawnerModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/WaveSpawnerModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/WeaponModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/WeaponModule.h

    ${CMAKE_SOURCE_DIR}/src/game/pathing/AIPath.cpp
    ${CMAKE_SOURCE_DIR}/src/game/pathing/AIPath.h
    ${CMAKE_SOURCE_DIR}/src/game/pathing/BreadthFirstSearch.cpp
    ${CMAKE_SOURCE_DIR}/src/game/pathing/BreadthFirstSearch.h
    ${CMAKE_SOURCE_DIR}/src/game/pathing/CostMap.cpp
    ${CMAKE_SOURCE_DIR}/src/game/pathing/CostMap.h
#     ${CMAKE_SOURCE_DIR}/src/game/pathing/PathFinder.cpp
#     ${CMAKE_SOURCE_DIR}/src/game/pathing/PathFinder.h

#     ${CMAKE_SOURCE_DIR}/src/input/AbstractController.cpp
#     ${CMAKE_SOURCE_DIR}/src/input/AbstractController.h
#     ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.cpp
#     ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.h

    ${CMAKE_SOURCE_DIR}/src/math/BasicMath.h
    ${CMAKE_SOURCE_DIR}/src/math/Spline.cpp
    ${CMAKE_SOURCE_DIR}/src/math/Spline.h

    ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h
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
    ${REAPER_RENDERER_BIN})

set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_BIN})

if (REAPER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if (REAPER_BUILD_KDEV_TEMPLATES)
    add_subdirectory(tools/kdevelop)
endif()
