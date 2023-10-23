#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_input)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/GenericController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GenericController.h
)

if(UNIX)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/LinuxController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/LinuxController.h
    )
endif()

target_link_libraries(${target}
    reaper_core
    fmt
)

reaper_configure_library(${target} "Input")

# reaper_add_tests(${target})
