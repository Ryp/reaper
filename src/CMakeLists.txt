#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

# Generate version header
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/core/Version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/core/Version.h)

add_subdirectory(core)
add_subdirectory(math)
add_subdirectory(vulkan_loader)
add_subdirectory(profiling)
add_subdirectory(mesh)
add_subdirectory(input)
add_subdirectory(common)
add_subdirectory(audio)
add_subdirectory(renderer)

add_subdirectory(neptune)

# Main executable
set(REAPER_BIN reaper)

add_executable(${REAPER_BIN})

target_sources(${REAPER_BIN} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GameLoop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GameLoop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TestTiledLighting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TestTiledLighting.h
)

target_link_libraries(${REAPER_BIN} PRIVATE
    reaper_core
    reaper_common
    reaper_math
    reaper_profiling
    reaper_renderer
    reaper_mesh
    reaper_input
    reaper_audio
    neptune_sim
    neptune_trackgen
    cgltf
)

# Configure main VS projet
set_target_properties(${REAPER_BIN} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${REAPER_BIN})

reaper_configure_executable(${REAPER_BIN} "Reaper")
