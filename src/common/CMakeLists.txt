#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_common)
set(REAPER_COMMON_BIN ${target} PARENT_SCOPE)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugLog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugLog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ReaperRoot.h
)

target_link_libraries(${target} PUBLIC
    ${REAPER_CORE_BIN}
    fmt
)

reaper_configure_library(${target} "Common")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/log.cpp
)
