#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_input)
set(REAPER_INPUT_BIN ${target} PARENT_SCOPE)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AbstractController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AbstractController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DS3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DS3.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DS4.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DS4.h
)

target_link_libraries(${target} PUBLIC
)

reaper_configure_library(${target} "Input")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

# reaper_add_tests(${target})
