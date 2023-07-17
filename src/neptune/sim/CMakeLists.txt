#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target neptune_sim)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/BulletConversion.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/SimExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsSim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsSim.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsSimUpdate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsSimUpdate.h
)

target_link_libraries(${target} PUBLIC
    reaper_core
    reaper_common
    glm
    span
    fmt
)

if(REAPER_USE_BULLET_PHYSICS)
    include(bullet)
    target_compile_definitions(${target} PUBLIC REAPER_USE_BULLET_PHYSICS)
    target_link_libraries(${target} PRIVATE bullet)
endif()

set_target_properties(${target} PROPERTIES FOLDER Neptune)

reaper_configure_library(${target} "Sim")

#reaper_add_tests(${target}
#    ${CMAKE_CURRENT_SOURCE_DIR}/test/TODO.cpp
#)
