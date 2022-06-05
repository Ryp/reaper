#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_profiling)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Scope.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scope.h
)

# Setup microprofile
if(REAPER_USE_MICROPROFILE)
    include(microprofile)
    target_compile_definitions(${target} PUBLIC REAPER_USE_MICROPROFILE)
    target_link_libraries(${target} PUBLIC microprofile)
endif()

reaper_configure_library(${target} "Profiling")
