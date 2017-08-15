#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(REAPER_GAMELOGIC_LIBRARIES ${REAPER_GAMELOGIC_LIBRARIES}
    ${REAPER_CORE_BIN})

set(REAPER_GAMELOGIC_INCLUDES ${REAPER_GAMELOGIC_INCLUDES}
    ${CMAKE_SOURCE_DIR}/external/glm)

set(REAPER_GAMELOGIC_BIN reaper_gamelogic)
set(REAPER_GAMELOGIC_BIN ${REAPER_GAMELOGIC_BIN} PARENT_SCOPE)

set(REAPER_GAMELOGIC_DIR ${CMAKE_SOURCE_DIR}/src/gamelogic)
set(REAPER_GAMELOGIC_SRCS
    ${REAPER_GAMELOGIC_DIR}/EntityDescriptor.h
    ${REAPER_GAMELOGIC_DIR}/GameLogicExport.h
    ${REAPER_GAMELOGIC_DIR}/ModuleUpdater.cpp
    ${REAPER_GAMELOGIC_DIR}/ModuleUpdater.h
    ${REAPER_GAMELOGIC_DIR}/WorldUpdater.cpp
    ${REAPER_GAMELOGIC_DIR}/WorldUpdater.h

    ${REAPER_GAMELOGIC_DIR}/entitydb/EntityDb.cpp
    ${REAPER_GAMELOGIC_DIR}/entitydb/EntityDb.h

    ${REAPER_GAMELOGIC_DIR}/map/MapDescriptor.h
    ${REAPER_GAMELOGIC_DIR}/map/MapInfo.cpp
    ${REAPER_GAMELOGIC_DIR}/map/MapInfo.h

    ${REAPER_GAMELOGIC_DIR}/module/DamageModule.cpp
    ${REAPER_GAMELOGIC_DIR}/module/DamageModule.h
    ${REAPER_GAMELOGIC_DIR}/module/MovementModule.cpp
    ${REAPER_GAMELOGIC_DIR}/module/MovementModule.h
    ${REAPER_GAMELOGIC_DIR}/module/PathModule.cpp
    ${REAPER_GAMELOGIC_DIR}/module/PathModule.h
    ${REAPER_GAMELOGIC_DIR}/module/PositionModule.cpp
    ${REAPER_GAMELOGIC_DIR}/module/PositionModule.h
    ${REAPER_GAMELOGIC_DIR}/module/TeamModule.cpp
    ${REAPER_GAMELOGIC_DIR}/module/TeamModule.h
    ${REAPER_GAMELOGIC_DIR}/module/TowerControllerModule.cpp
    ${REAPER_GAMELOGIC_DIR}/module/TowerControllerModule.h
    ${REAPER_GAMELOGIC_DIR}/module/WaveSpawnerModule.cpp
    ${REAPER_GAMELOGIC_DIR}/module/WaveSpawnerModule.h
    ${REAPER_GAMELOGIC_DIR}/module/WeaponModule.cpp
    ${REAPER_GAMELOGIC_DIR}/module/WeaponModule.h

    ${REAPER_GAMELOGIC_DIR}/pathing/AIPath.cpp
    ${REAPER_GAMELOGIC_DIR}/pathing/AIPath.h
    ${REAPER_GAMELOGIC_DIR}/pathing/BreadthFirstSearch.cpp
    ${REAPER_GAMELOGIC_DIR}/pathing/BreadthFirstSearch.h
    ${REAPER_GAMELOGIC_DIR}/pathing/CostMap.cpp
    ${REAPER_GAMELOGIC_DIR}/pathing/CostMap.h
#     ${REAPER_GAMELOGIC_DIR}/pathing/PathFinder.cpp
#     ${REAPER_GAMELOGIC_DIR}/pathing/PathFinder.h
)

set(REAPER_GAMELOGIC_TEST_SRCS
    ${REAPER_GAMELOGIC_DIR}/test/cost_map.cpp
    ${REAPER_GAMELOGIC_DIR}/test/world_updater.cpp
)

add_library(${REAPER_GAMELOGIC_BIN} ${REAPER_BUILD_TYPE} ${REAPER_GAMELOGIC_SRCS})
reaper_configure_target(${REAPER_GAMELOGIC_BIN} "GameLogic")

target_include_directories(${REAPER_GAMELOGIC_BIN} PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${REAPER_GAMELOGIC_BIN} SYSTEM PUBLIC ${REAPER_GAMELOGIC_INCLUDES})

#Libraries
target_link_libraries(${REAPER_GAMELOGIC_BIN} ${REAPER_GAMELOGIC_LIBRARIES})

set_target_properties(${REAPER_GAMELOGIC_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_GAMELOGIC_BIN})

if (REAPER_BUILD_TESTS)
    reaper_add_tests(${REAPER_GAMELOGIC_BIN} ${REAPER_GAMELOGIC_TEST_SRCS})
endif()
