#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_gamelogic)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/EntityDescriptor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GameLogicExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ModuleUpdater.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ModuleUpdater.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldUpdater.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldUpdater.h

    ${CMAKE_CURRENT_SOURCE_DIR}/entitydb/EntityDb.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/entitydb/EntityDb.h

    ${CMAKE_CURRENT_SOURCE_DIR}/map/MapDescriptor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/map/MapInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/map/MapInfo.h

    ${CMAKE_CURRENT_SOURCE_DIR}/module/DamageModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/DamageModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/MovementModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/MovementModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/PathModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/PathModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/PositionModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/PositionModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/TeamModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/TeamModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/TowerControllerModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/TowerControllerModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/WaveSpawnerModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/WaveSpawnerModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/WeaponModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/WeaponModule.h

    ${CMAKE_CURRENT_SOURCE_DIR}/pathing/AIPath.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pathing/AIPath.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pathing/BreadthFirstSearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pathing/BreadthFirstSearch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pathing/CostMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pathing/CostMap.h
#     ${CMAKE_CURRENT_SOURCE_DIR}/pathing/PathFinder.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/pathing/PathFinder.h
)

target_link_libraries(${target} PUBLIC
    reaper_core
    glm
)

reaper_configure_library(${target} "GameLogic")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/cost_map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/world_updater.cpp
)
