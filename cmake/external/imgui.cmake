#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(IMGUI_BIN imgui)

add_library(${IMGUI_BIN} ${REAPER_BUILD_TYPE})

set(REAPER_IMGUI_CONFIG imgui_config.reaper.h)
target_compile_definitions(${IMGUI_BIN} PUBLIC IMGUI_USER_CONFIG="${REAPER_IMGUI_CONFIG}")

set(IMGUI_PATH ${CMAKE_SOURCE_DIR}/external/imgui)

target_sources(${IMGUI_BIN} PRIVATE
    ${IMGUI_PATH}/imgui.cpp
    ${IMGUI_PATH}/imgui.h
    ${IMGUI_PATH}/imgui_demo.cpp
    ${IMGUI_PATH}/imgui_draw.cpp
    ${IMGUI_PATH}/imgui_widgets.cpp
    ${IMGUI_PATH}/imgui_tables.cpp
    ${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_PATH}/backends/imgui_impl_vulkan.h
)

set(IMGUI_INCLUDE_DIR ${IMGUI_PATH})

configure_file(${CMAKE_SOURCE_DIR}/external/imgui_config.h.in ${IMGUI_INCLUDE_DIR}/${REAPER_IMGUI_CONFIG})

target_link_libraries(${IMGUI_BIN} PRIVATE reaper_vulkan_loader)

target_include_directories(${IMGUI_BIN} PRIVATE ${IMGUI_INCLUDE_DIR})

target_include_directories(${IMGUI_BIN} SYSTEM INTERFACE ${IMGUI_INCLUDE_DIR})

reaper_configure_external_target(${IMGUI_BIN} "ImGUI" ${IMGUI_PATH})

set_target_properties(${IMGUI_BIN} PROPERTIES FOLDER External)
