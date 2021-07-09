#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

# Set precompiled header
set(REAPER_PCH ${CMAKE_CURRENT_SOURCE_DIR}/pch/stdafx.h)

# Generate version header
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/core/Version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/core/Version.h NEWLINE_STYLE UNIX)

add_subdirectory(core)
add_subdirectory(math)
add_subdirectory(mesh)
add_subdirectory(input)
add_subdirectory(common)
add_subdirectory(gamelogic)
add_subdirectory(renderer)

add_subdirectory(splinesonic)

# Main executable
set(REAPER_BIN reaper)

add_executable(${REAPER_BIN})

target_sources(${REAPER_BIN} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/AssetCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetCache.h

    ${CMAKE_CURRENT_SOURCE_DIR}/cvar/CVar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cvar/CVar.h

#     ${CMAKE_CURRENT_SOURCE_DIR}/input/AbstractController.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/input/AbstractController.h
#     ${CMAKE_CURRENT_SOURCE_DIR}/input/SixAxisController.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/input/SixAxisController.h

    ${CMAKE_CURRENT_SOURCE_DIR}/pch/stdafx.h

    ${CMAKE_CURRENT_SOURCE_DIR}/resource/Resource.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/Resource.h
)

target_link_libraries(${REAPER_BIN} PRIVATE
    ${REAPER_CORE_BIN}
    ${REAPER_COMMON_BIN}
    ${REAPER_GAMELOGIC_BIN}
    ${REAPER_MATH_BIN}
    ${REAPER_MESH_BIN}
    ${REAPER_RENDERER_BIN}
    ${REAPER_INPUT_BIN}
)

# Configure main VS projet
set_target_properties(${REAPER_BIN} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${REAPER_BIN})

reaper_configure_executable(${REAPER_BIN} "Reaper")

set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_BIN})
