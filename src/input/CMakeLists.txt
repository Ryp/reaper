#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_input)

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

# reaper_add_tests(${target})
