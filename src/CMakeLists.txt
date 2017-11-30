#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

# Main executable
set(REAPER_BIN reaper)

# Set precompiled header
set(REAPER_PCH ${CMAKE_CURRENT_SOURCE_DIR}/pch/stdafx.h)

# Generate version header
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/core/Version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/core/Version.h NEWLINE_STYLE UNIX)

# Choose windowing system
if(WIN32)
    set(REAPER_USE_WIN32_WINDOW ON)
    set(VK_PLATFORM "VK_USE_PLATFORM_WIN32_KHR")
elseif(APPLE OR UNIX)
    set(REAPER_USE_XCB_WINDOW ON)
    set(VK_PLATFORM "VK_USE_PLATFORM_XCB_KHR")
    #set(REAPER_USE_XLIB_WINDOW ON)
    #set(VK_PLATFORM "VK_USE_PLATFORM_XLIB_KHR")
else()
    # We could allow headless instances without problems
    message(FATAL_ERROR "Could not detect platform!")
endif()
add_definitions(-D${VK_PLATFORM})

add_subdirectory(core)
add_subdirectory(math)
add_subdirectory(mesh)
add_subdirectory(common)
add_subdirectory(gamelogic)
add_subdirectory(renderer)

add_subdirectory(splinesonic)

set(REAPER_SRCS
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

# List additionnal natvis files for Visual Studio
if(MSVC)
    set(REAPER_NATVIS_FILES
        ${CMAKE_SOURCE_DIR}/external/glm/util/glm.natvis
    )
endif()

add_executable(${REAPER_BIN} ${REAPER_SRCS} ${REAPER_NATVIS_FILES})
reaper_configure_executable(${REAPER_BIN} "Reaper")

# Configure main VS projet
set_target_properties(${REAPER_BIN} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${REAPER_BIN})

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_BIN} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_BIN} SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/external)

#Libraries
target_link_libraries(${REAPER_BIN}
    ${MICROPROFILE_BIN}
    ${REAPER_CORE_BIN}
    ${REAPER_COMMON_BIN}
    ${REAPER_GAMELOGIC_BIN}
    ${REAPER_MATH_BIN}
    ${REAPER_MESH_BIN}
    ${REAPER_RENDERER_BIN})

set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_BIN})
