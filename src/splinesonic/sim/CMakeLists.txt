#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target splinesonic_sim)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/SimExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Test.h
)

target_link_libraries(${target} PUBLIC
    reaper_core
    glm
    span
)

if(REAPER_USE_BULLET_PHYSICS)
    include(bullet)
    target_link_libraries(${target} PRIVATE bullet)
endif()

set_target_properties(${target} PROPERTIES FOLDER SplineSonic)

reaper_configure_library(${target} "Sim")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

#reaper_add_tests(${target}
#    ${CMAKE_CURRENT_SOURCE_DIR}/test/TODO.cpp
#)
